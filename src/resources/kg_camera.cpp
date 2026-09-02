//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "core/kg_core.hpp"

#include "vulkan/vulkan_core.h"
KG_VK_MEM_ALLOC_IGNORE_PUSH
#include "vma/vk_mem_alloc.h"
KG_VK_MEM_ALLOC_IGNORE_POP

#include "log_utils.hpp"

#include "resources/kg_camera.hpp"
#include "core/kg_context.hpp"
#include "core/kg_viewport.hpp"
#include "resources/kg_mesh.hpp"
#include "resources/kg_shader.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::DIR_UP;
using KalaHeaders::KalaMath::PosTarget;
using KalaHeaders::KalaMath::view;
using KalaHeaders::KalaMath::ortho;
using KalaHeaders::KalaMath::perspective;
using KalaHeaders::KalaMath::isnear;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;
using KalaGraphics::Core::Viewport;

using std::unique_ptr;
using std::make_unique;
using std::to_string;

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Camera> registry{};

    KalaGraphicsRegistry<Camera>& Camera::GetRegistry() { return registry; }

    Camera* Camera::Initialize(
        u32 shaderID,
        CameraType newType)
    {
        Shader* shader{};
        string err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            Log::Print(
                "Failed to create camera because of invalid shader! Reason: " + err,
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (shader->is2D != (newType == CameraType::CAM_ORTHOGRAPHIC))
        {
            Log::Print(
                "Failed to create camera because camera type is not compatible with shader 2D state!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        Viewport* vp{};
        err = Viewport::GetRegistry().GetContent(shader->viewportID, vp);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics camera error",
                "Failed to create camera because the shaders viewport was invalid! Reason: " + err);
        }

        GraphicsContext* gctx{};
        err = GraphicsContext::GetRegistry().GetContent(vp->contextID, gctx);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics camera error",
                "Failed to create camera because the graphics context "
                "on the cameras viewport was invalid! Reason: " + err);
        }

        //TODO: figure out a better solution
        if (shader->descriptorSetLayouts.empty())
        {
            Log::Print(
                "Failed to create camera because the shader '" 
                + to_string(shaderID) + "' had no shader data!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<Camera> newCamera = make_unique<Camera>();
        Camera* cameraPtr = newCamera.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        cameraPtr->ID = newID;
        cameraPtr->shaderID = shaderID;

        //shader references this camera
        shader->cameraIDs.push_back(newID);

        cameraPtr->type = newType;

        if (cameraPtr->type == CameraType::CAM_ORTHOGRAPHIC)
        {
            cameraPtr->drawDistance = { -1, 1 };
        }

        //always assign descriptor set data at camera init
        cameraPtr->isDirty = true;

        if (newType == CameraType::CAM_PERSPECTIVE)
        {
            if (vp->primary3DCameraID == 0) vp->primary3DCameraID = newID;
            else                            vp->extra3DCameraIDs.push_back(newID);

            cameraPtr->viewportID = vp->ID;
        }

        if (newType == CameraType::CAM_ORTHOGRAPHIC)
        {
            if (vp->primary2DCameraID == 0) vp->primary2DCameraID = newID;
            else                            vp->extra2DCameraIDs.push_back(newID);

            cameraPtr->viewportID = vp->ID;
        }

        //blank data for empty camera,
        //move also calls UpdateCameraData
        cameraPtr->Move({}, {});

        err = registry.AddContent(newID, std::move(newCamera));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics camera error",
				"Failed to initialize camera! Reason: " + err);
        }

        Log::Print(
			"Created new camera '" + to_string(newID) 
            + "' for shader '" + to_string(shaderID) + "'!",
			"KG_CAMERA",
			LogType::LOG_SUCCESS);

        return cameraPtr;
    }

    u32 Camera::GetID() const { return ID; }
    u32 Camera::GetViewportID() const { return viewportID; }
    u32 Camera::GetShaderID() const { return shaderID; }
    
    u32 Camera::GetMeshID() const { return meshID; }
    void Camera::SetMeshID(u32 newValue)
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics camera error",
                "Failed to set camera '" + to_string(ID) 
                + "' mesh ID because the logical device was invalid!");
        }

        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();
        if (!allocator)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics camera error",
                "Failed to set camera '" + to_string(ID) + "' mesh ID "
                "because vma allocator was invalid!");
        }

        if (meshID == newValue)
        {
            Log::Print(
                "Failed to set camera '" + to_string(ID) 
                + "' mesh ID to '" + to_string(newValue) 
                + "' because it already is that value!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        Mesh* oldMesh{};
        string err = Mesh::GetRegistry().GetContent(meshID, oldMesh);
        if (meshID != 0
            && !err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics camera error",
                "Failed to set mesh ID for camera '" 
                + to_string(ID) + "' because of invalid old mesh! Reason: " + err);
        }

        Mesh* mesh{};
        err = Mesh::GetRegistry().GetContent(newValue, mesh);
        if (!err.empty())
        {
            Log::Print(
                "Failed to set camera '" + to_string(ID) 
                + "' mesh ID because it was invalid! Reason: " + err,
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (mesh->is2D)
        {
            Log::Print(
                "Failed to set camera '" + to_string(ID) 
                + "' mesh ID to '" + to_string(newValue) 
                + "' because 2D meshes cannot be added to any cameras!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (Is2D())
        {
            Log::Print(
                "Failed to set camera '" + to_string(ID) 
                + "' mesh ID to '" + to_string(newValue) 
                + "' because 2D cameras cannot be given a mesh!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        meshID = newValue;

        if (oldMesh) oldMesh->cameraID = 0;
        mesh->cameraID = ID;

        Shader* shader{};
        err = Shader::GetRegistry().GetContent(mesh->shaderID, shader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics camera error",
                "Failed to set mesh ID for camera '" 
                + to_string(ID) + "' because of invalid mesh shader! Reason: " + err);
        }

        //TODO: figure out what data needs to change/clear when modifying mesh ID

        if (vkDescriptorSet != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(
                logicalDevice,
                GraphicsContext::GetDescriptorPool(),
                1,
                &vkDescriptorSet);

            vkDescriptorSet = VK_NULL_HANDLE;
        }

        if (vmaCameraUBOAllocation != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(
                allocator,
                vkCameraUBOBuffer,
                vmaCameraUBOAllocation);

            vkCameraUBOBuffer = VK_NULL_HANDLE;
            vmaCameraUBOAllocation = VK_NULL_HANDLE;
            cameraUBOMappedPtr = nullptr;
        }

        if (meshID != 0)
        {
            VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
            descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptorSetAllocateInfo.descriptorPool = GraphicsContext::GetDescriptorPool();
            descriptorSetAllocateInfo.descriptorSetCount = 1;
            descriptorSetAllocateInfo.pSetLayouts = &shader->descriptorSetLayouts[1];

            VkDescriptorSet newDescriptorSet;
            VkResult vkResult = vkAllocateDescriptorSets(
                logicalDevice,
                &descriptorSetAllocateInfo,
                &newDescriptorSet);

            if (vkResult != VK_SUCCESS)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics camera error",
                    "Failed to set camera '" + to_string(ID) 
                    + "' mesh ID because descriptor set init failed! Reason: " 
                    + GraphicsContext::GetVkResultMessage(vkResult));
            }

            vkDescriptorSet = newDescriptorSet;

            //reassign descriptor set data because we have a new mesh
            isDirty = true;

            Log::Print(
                "Set camera '" + to_string(ID) 
                + "' mesh ID to '" + to_string(meshID) + "'!",
                "KG_CAMERA",
                LogType::LOG_SUCCESS);
        }
        else
        {
            Log::Print(
                "Cleared camera '" + to_string(ID) + "' mesh ID.",
                "KG_CAMERA",
                LogType::LOG_INFO);
        }
    }

    void Camera::Move(
        vec2 mouse,
        vec2 keyboard,
        f32 vertical,
        f32 deltaTime)
    {
        Viewport* vp{};
        string err = Viewport::GetRegistry().GetContent(viewportID, vp);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics camera error",
                "Failed to update camera '" + to_string(ID) 
                + "' data because its viewport was invalid! Reason: " + err);
        }

        if (!Is2D()) vp->is3DMeshSortDirty = true;

        mouse = kclamp(mouse, -MOUSE_MAX, MOUSE_MAX);
        keyboard = kclamp(keyboard, -1, 1);

        if (type == CameraType::CAM_ORTHOGRAPHIC)
        {
            vec3 move = vec2(
                keyboard.x * speedMultiplier,
                keyboard.y * speedMultiplier);

            transform.addpos(move);

            orthographicMatrix = ortho(
                true,
                vp->viewportDynamicSize,
                drawDistance.x,
                drawDistance.y);
        }
        else
        {
            if (!isnear(mouse.x)
                || !isnear(mouse.y))
            {
                transform.addyaw(-mouse.x * sensitivityMultiplier);
                transform.addpitch(-mouse.y * sensitivityMultiplier);
            }

            vec3 kb3{keyboard.x, vertical, keyboard.y};
            if (kb3.x != 0
                || kb3.y != 0
                || kb3.z != 0)
            {
                f32 len = sqrt(kb3.x * kb3.x + kb3.y * kb3.y + kb3.z * kb3.z);
                kb3 /= len;
            }

            vec3 move = transform.getdirfront()
                * kb3.z
                + transform.getdirright()
                * kb3.x;

            move += DIR_UP * kb3.y;

            move *= speedMultiplier * deltaTime;

            transform.addpos(move);

            vec3 pos = transform.getpos(PosTarget::POS_WORLD);

            mat4 viewMatrix = view(
                pos,
                pos + transform.getdirfront(),
                DIR_UP);

            mat4 perspectiveMatrix = perspective(
                true,
                vp->viewportDynamicSize,
                fov,
                drawDistance.x,
                drawDistance.y);

            projectionMatrix = perspectiveMatrix * viewMatrix;

            /*
            string pmStr = "projection matrix:\n" +
                to_string(projectionMatrix.m00) + " " + to_string(projectionMatrix.m10) + " " + to_string(projectionMatrix.m20) + " " + to_string(projectionMatrix.m30) + "\n" +
                to_string(projectionMatrix.m01) + " " + to_string(projectionMatrix.m11) + " " + to_string(projectionMatrix.m21) + " " + to_string(projectionMatrix.m31) + "\n" +
                to_string(projectionMatrix.m02) + " " + to_string(projectionMatrix.m12) + " " + to_string(projectionMatrix.m22) + " " + to_string(projectionMatrix.m32) + "\n" +
                to_string(projectionMatrix.m03) + " " + to_string(projectionMatrix.m13) + " " + to_string(projectionMatrix.m23) + " " + to_string(projectionMatrix.m33) + "\n";

            string tpos = "transform world pos:\n" + 
                to_string(transform.pos_world.x) + ", " +
                to_string(transform.pos_world.y) + ", " +
                to_string(transform.pos_world.z);

            Log::Print(pmStr + ", " + tpos);
            */
        }
    }

    Transform3D& Camera::GetTransform() { return transform; }

    CameraType Camera::GetCameraType() const { return type; }

    bool Camera::Is2D() { return type == CameraType::CAM_ORTHOGRAPHIC; }

    f32 Camera::GetSpeedMultiplier() const { return speedMultiplier; }
    void Camera::SetSpeedMultiplier(f32 newValue)
    {
        if (newValue < SPEED_MIN
            || newValue > SPEED_MAX)
        {
            Log::Print(
                "Failed to set camera '" + to_string(ID) + "' speed multiplier because new value is out of allowed range!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        speedMultiplier = newValue;

        Log::Print(
            "Set camera '" + to_string(ID) + "' speed multiplier to '" + to_string(speedMultiplier) + "'.",
            "KG_CAMERA",
            LogType::LOG_SUCCESS);
    }

    f32 Camera::GetSensitivityMultiplier() const { return sensitivityMultiplier; }
    void Camera::SetSensitivityMultiplier(f32 newValue)
    {
        if (newValue < SENS_MIN
            || newValue > SENS_MAX)
        {
            Log::Print(
                "Failed to set camera '" + to_string(ID) + "' sensitivity multiplier because new value is out of allowed range!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        sensitivityMultiplier = newValue;

        Log::Print(
            "Set camera '" + to_string(ID) + "' sensitivity multiplier to '" + to_string(sensitivityMultiplier) + "'.",
            "KG_CAMERA",
            LogType::LOG_SUCCESS);
    }

    f32 Camera::GetFOV() const { return fov; }
    void Camera::SetFOV(f32 newValue)
    {
        if (newValue < FOV_MIN
            || newValue > FOV_MAX)
        {
            Log::Print(
                "Failed to set camera '" + to_string(ID) + "' fov because new value is out of allowed range!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        fov = newValue;

        Log::Print(
            "Set camera '" + to_string(ID) + "' fov to '" + to_string(fov) + "'.",
            "KG_CAMERA",
            LogType::LOG_SUCCESS);
    }

    vec2 Camera::GetDrawDistance() const { return drawDistance; }
    void Camera::SetDrawDistance(vec2 newValue)
    {
        if (type == CameraType::CAM_ORTHOGRAPHIC)
        {
            Log::Print(
                "Failed to set camera '" + to_string(ID) 
                + "' draw distance because it is an orthographic camera!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (newValue < DRAW_DISTANCE_MIN
            || newValue > DRAW_DISTANCE_MAX)
        {
            Log::Print(
                "Failed to set camera '" + to_string(ID) 
                + "' draw distance because new value is out of allowed range!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (newValue.x >= newValue.y)
        {
            Log::Print(
                "Failed to set camera '" + to_string(ID) 
                + "' draw distance because near distance cannot be equal to or more than far distance!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        drawDistance = newValue;

        Log::Print(
            "Set camera '" + to_string(ID) + "' draw distance to '" + to_string(drawDistance.x) + ", " + to_string(drawDistance.y) + "'.",
            "KG_CAMERA",
            LogType::LOG_SUCCESS);
    }

    const mat4& Camera::GetMatrix() const
    { 
        return type == CameraType::CAM_ORTHOGRAPHIC
            ? orthographicMatrix
            : projectionMatrix;
    }

    void Camera::ClearAllData()
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics camera error",
                "Failed to clear camera '" + to_string(ID) 
                + "' data because the logical device was invalid!");
        }

        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();
        if (!allocator)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics camera error",
                "Failed to clear camera '" + to_string(ID) + "' data "
                "because the vma allocator was invalid!");
        }

        //drain the gpu before destroying this camera
        VkResult vkResult = vkDeviceWaitIdle(logicalDevice);
        if (vkResult != VK_SUCCESS)
        {
            GraphicsContext::ForceClose(
                "KalaGraphics camera error",
                "Failed to clear camera '" + to_string(ID) 
                + "' data because vkDeviceWaitIdle did not succeed!",
                vkResult);
        }

        if (vmaCameraUBOAllocation != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(
                allocator,
                vkCameraUBOBuffer,
                vmaCameraUBOAllocation);

            cameraUBOMappedPtr = nullptr;
        }

        if (vkDescriptorSet != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(
                logicalDevice,
                GraphicsContext::GetDescriptorPool(),
                1,
                &vkDescriptorSet);
        }
    }

    void Camera::UpdateCameraData()
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics camera error",
                "Failed to update camera '" + to_string(ID) 
                + "' data because the logical device was invalid!");
        }

        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();
        if (!allocator)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics camera error",
                "Failed to update camera '" + to_string(ID) + "' data "
                "because the vma allocator was invalid!");
        }

        Shader* shader{};
        string err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics camera error",
                "Failed to update camera '" + to_string(ID) 
                + "' data because its shader was invalid! Reason: " + err);
        }

        if (vkDescriptorSet == VK_NULL_HANDLE)
        {
            //
            // DESCRIPTOR SET
            //

            VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
            descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptorSetAllocateInfo.descriptorPool = GraphicsContext::GetDescriptorPool();
            descriptorSetAllocateInfo.descriptorSetCount = 1;
            descriptorSetAllocateInfo.pSetLayouts = &shader->descriptorSetLayouts[0];

            VkDescriptorSet newDescriptorSet;
            VkResult vkResult = vkAllocateDescriptorSets(
                logicalDevice,
                &descriptorSetAllocateInfo,
                &newDescriptorSet);

            if (vkResult != VK_SUCCESS)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics camera update error",
                    "Failed to update camera because descriptor set init failed! Reason: " 
                    + GraphicsContext::GetVkResultMessage(vkResult));
            }

            vkDescriptorSet = newDescriptorSet;

            //
            // VMA ALLOCATOR
            //

            size_t bufferSize = sizeof(mat4);

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = bufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            allocInfo.flags = 
                VMA_ALLOCATION_CREATE_MAPPED_BIT
                | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

            VkBuffer newBuffer{};
            VmaAllocation newAllocation{};
            VmaAllocationInfo allocResult{};

            VkResult result = vmaCreateBuffer(
                allocator,
                &bufferInfo,
                &allocInfo,
                &newBuffer,
                &newAllocation,
                &allocResult);

            if (result != VK_SUCCESS)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics camera update error",
                    "Failed to update camera because vma allocator init failed! Reason: " 
                    + GraphicsContext::GetVkResultMessage(vkResult));
            }

            vkCameraUBOBuffer = newBuffer;
            vmaCameraUBOAllocation = newAllocation;
            cameraUBOMappedPtr = allocResult.pMappedData;
        }

        memcpy(
            cameraUBOMappedPtr,
            &GetMatrix(),
            sizeof(mat4));

        if (isDirty)
        {
            VkDescriptorBufferInfo transformInfo{};
            transformInfo.buffer = vkCameraUBOBuffer;
            transformInfo.offset = 0;
            transformInfo.range = sizeof(mat4);

            VkWriteDescriptorSet writes[1]{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = vkDescriptorSet;
            writes[0].dstBinding = 0; // <<<< SET 0 BINDING 0 - CAMERA UBO SLOT
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].pBufferInfo = &transformInfo;

            vkUpdateDescriptorSets(
                logicalDevice,
                1,
                writes,
                0,
                nullptr);

            isDirty = false;
        }
    }

    void Camera::Destroy()
    {
        Mesh* mesh{};
        string err = Mesh::GetRegistry().GetContent(meshID, mesh);
        if (err.empty()) mesh->cameraID = 0;

        Shader* shader{};
        err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (err.empty())
        {
            erase(
                shader->cameraIDs, 
                ID);
        }

        err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics camera error",
                "Failed to destroy camera '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    Camera::~Camera()
    {
        Log::Print(
            "Destroying camera '" + to_string(ID) + "'.",
            "KG_CAMERA",
            LogType::LOG_INFO);

        ClearAllData();
    }
}