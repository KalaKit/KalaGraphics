//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "vulkan/vulkan_core.h"
#include "vma/vk_mem_alloc.h"

#include "log_utils.hpp"

#include "resources/kg_camera.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"
#include "resources/kg_mesh.hpp"
#include "resources/kg_shader.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::DIR_UP;
using KalaHeaders::KalaMath::PosTarget::POS_WORLD;
using KalaHeaders::KalaMath::RotTarget::ROT_WORLD;
using KalaHeaders::KalaMath::addpos3d;
using KalaHeaders::KalaMath::addyaw;
using KalaHeaders::KalaMath::addpitch;
using KalaHeaders::KalaMath::addpos3d;
using KalaHeaders::KalaMath::getdirfront;
using KalaHeaders::KalaMath::getdirright;
using KalaHeaders::KalaMath::view;
using KalaHeaders::KalaMath::ortho;
using KalaHeaders::KalaMath::perspective;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;

using std::unique_ptr;
using std::make_unique;
using std::to_string;

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Camera> registry{};

    KalaGraphicsRegistry<Camera>& Camera::GetRegistry() { return registry; }

    Camera* Camera::Initialize(
        u32 contextID,
        u32 shaderID)
    {
        GraphicsContext* gctx = GraphicsContext::GetRegistry().GetContent(contextID);
        if (!gctx)
        {
            Log::Print(
                "Failed to create camera because the graphics context '" 
                + to_string(contextID) + "' was invalid!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        Shader* shader = Shader::GetRegistry().GetContent(shaderID);
        if (!shader)
        {
            Log::Print(
                "Failed to create camera because the shader '" 
                + to_string(shaderID) + "' was invalid!",
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

        setroteuler(
            cameraPtr->transform,
            {},
            ROT_WORLD,
            { 0.0f, -90.0f, 0.0f });

        cameraPtr->viewport = gctx->GetExtent();

        //graphics context references this camera
        gctx->cameraIDs.push_back(newID);

        registry.AddContent(newID, std::move(newCamera));

        Log::Print(
			"Created new camera '" + to_string(newID) 
            + "' for graphics context '" + to_string(contextID) + "'!",
			"KG_CAMERA",
			LogType::LOG_SUCCESS);

        return cameraPtr;
    }

    u32 Camera::GetID() const { return ID; }

    u32 Camera::GetGraphicsContextID() const { return contextID; }
    void Camera::SetGraphicsContextID(u32 newValue)
    {
        if (contextID == newValue)
        {
            Log::Print("Failed to set camera '" + to_string(ID) 
                + "' graphics context ID to '" + to_string(newValue) 
                + "' because it already is that value!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        GraphicsContext* oldContext = GraphicsContext::GetRegistry().GetContent(meshID);
        GraphicsContext* gctx = GraphicsContext::GetRegistry().GetContent(newValue);
        if (!gctx)
        {
            Log::Print("Failed to set camera '" + to_string(ID) 
                + "' graphics context ID to '" + to_string(newValue) 
                + "' because it was invalid!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        contextID = newValue;

        if (oldContext)
        {
            erase(
                oldContext->cameraIDs,
                ID);
        }
        gctx->cameraIDs.push_back(ID);

        Log::Print(
            "Set camera '" + to_string(ID) 
            + "' graphics context ID to '" + to_string(contextID) + "'!",
            "KG_CAMERA",
            LogType::LOG_SUCCESS);
    }

    u32 Camera::GetMeshID() const { return meshID; }
    void Camera::SetMeshID(u32 newValue)
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to set camera '" + to_string(ID) 
                + "' mesh ID because the logical device was invalid!");
        }

        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();
        if (!allocator)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to set camera '" + to_string(ID) + "' mesh ID "
                "because vma allocator was invalid!");
        }

        if (meshID == newValue)
        {
            Log::Print("Failed to set camera '" + to_string(ID) 
                + "' mesh ID to '" + to_string(newValue) 
                + "' because it already is that value!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        Mesh* oldMesh = Mesh::GetRegistry().GetContent(meshID);
        Mesh* mesh = Mesh::GetRegistry().GetContent(newValue);
        if (!mesh)
        {
            Log::Print("Failed to set camera '" + to_string(ID) 
                + "' mesh ID to '" + to_string(newValue) 
                + "' because it was invalid!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        meshID = newValue;

        if (oldMesh) oldMesh->cameraID = 0;
        mesh->cameraID = ID;

        Shader* shader = Shader::GetRegistry().GetContent(mesh->shaderID);

        if (vkCameraDescriptorSet != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(
                logicalDevice,
                GraphicsContext::GetDescriptorPool(),
                1,
                &vkCameraDescriptorSet);

            vkCameraDescriptorSet = VK_NULL_HANDLE;
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
            descriptorSetAllocateInfo.pSetLayouts = &shader->descriptorSetLayout;

            VkDescriptorSet newDescriptorSet;
            VkResult vkResult = vkAllocateDescriptorSets(
                logicalDevice,
                &descriptorSetAllocateInfo,
                &newDescriptorSet);

            if (vkResult != VK_SUCCESS)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to set camera '" + to_string(ID) 
                    + "' mesh ID because descriptor set init failed! Reason: " 
                    + GraphicsContext::GetVkResultMessage(vkResult));
            }

            vkCameraDescriptorSet = newDescriptorSet;

            recreateBuffer = true;
            UpdateCameraData();

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

    CameraType Camera::GetCameraType() const { return type; }
    void Camera::SetCameraType(CameraType newValue)
    {
        switch (newValue)
        {
        case CameraType::C_INVALID:
            Log::Print(
                "Failed to set camera '" + to_string(ID) 
                + "' type because it was invalid!");
            break;
        case CameraType::C_ORTHOGRAPHIC:
            type = CameraType::C_ORTHOGRAPHIC;
            break;
        case CameraType::C_PERSPECTIVE:
            type = CameraType::C_PERSPECTIVE;
            break;
        }
    }

    void Camera::Move(
        vec2 mouse,
        vec2 keyboard)
    {
        vec2 kmouse = kclamp(mouse, 0, 1);
        vec2 kkb = kclamp(keyboard, 0, 1);

        if (type == CameraType::C_ORTHOGRAPHIC)
        {
            addpos3d(
                transform,
                {},
                POS_WORLD,
                vec2(kmouse + kkb));

            orthographicMatrix = ortho(
                viewport,
                drawDistance.x,
                drawDistance.y);
        }
        else
        {
            addyaw(
                transform,
                {},
                ROT_WORLD,
                kmouse.x);
            addpitch(
                transform,
                {},
                ROT_WORLD,
                kmouse.y);

            vec3 move = getdirfront(transform)
                * kkb.y
                + getdirright(transform)
                * kkb.x;

            addpos3d(
                transform,
                {},
                POS_WORLD,
                move);

            mat4 viewMatrix = view(
                transform.pos_world,
                getdirfront(transform),
                DIR_UP);

            mat4 perspectiveMatrix = perspective(
                viewport,
                fov,
                drawDistance.x,
                drawDistance.y);

            projectionMatrix = perspectiveMatrix * viewMatrix;
        }

        UpdateCameraData();
    }

    void Camera::UpdateCameraData()
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();

        Shader* shader = Shader::GetRegistry().GetContent(shaderID);
        if (!shader)
        {
            Log::Print(
                "Failed to update camera '" + to_string(ID) 
                + "' data because the shader '" + to_string(shaderID) + "' was invalid!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (vmaCameraUBOAllocation == VK_NULL_HANDLE)
        {
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

            VkBuffer vkBuffer = VK_NULL_HANDLE;
            VmaAllocation vmaAllocation = VK_NULL_HANDLE;
            VmaAllocationInfo allocResult{};

            VkResult result = vmaCreateBuffer(
                allocator,
                &bufferInfo,
                &allocInfo,
                &vkBuffer,
                &vmaAllocation,
                &allocResult);

            if (result != VK_SUCCESS)
            {
                Log::Print(
                    "Failed to update camera '" + to_string(ID) 
                    + "' UBO data because camera UBO vk buffer creation failed!",
                    "KG_CAMERA",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            vkCameraUBOBuffer = vkBuffer;
            vmaCameraUBOAllocation = vmaAllocation;
            cameraUBOMappedPtr = allocResult.pMappedData;
        }

        memcpy(
            cameraUBOMappedPtr,
            &GetCameraMatrix(),
            sizeof(mat4));

        if (recreateBuffer)
        {
            VkDescriptorBufferInfo transformInfo{};
            transformInfo.buffer = vkCameraUBOBuffer;
            transformInfo.offset = 0;
            transformInfo.range = sizeof(mat4);

            VkWriteDescriptorSet writes[1]{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = vkCameraDescriptorSet;
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].pBufferInfo = &transformInfo;

            vkUpdateDescriptorSets(
                logicalDevice,
                1,
                writes,
                0,
                nullptr);

            recreateBuffer = false;
        }
    }

    Transform3D& Camera::GetTransform() { return transform; }

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
    }

    vec2 Camera::GetDrawDistance() const { return drawDistance; }
    void Camera::SetDrawDistance(vec2 newValue)
    {
        if (newValue < DRAW_DISTANCE_MIN
            || newValue > DRAW_DISTANCE_MAX)
        {
            Log::Print(
                "Failed to set camera '" + to_string(ID) + "' draw distance because new value is out of allowed range!",
                "KG_CAMERA",
                LogType::LOG_ERROR,
                2);

            return;
        }

        drawDistance = newValue;
    }

    const mat4& Camera::GetCameraMatrix() const
    { 
        return type == CameraType::C_ORTHOGRAPHIC
            ? orthographicMatrix
            : projectionMatrix;
    }

    VkBuffer Camera::GetBuffer() { return vkCameraUBOBuffer; }
    VmaAllocation Camera::GetAllocation() { return vmaCameraUBOAllocation; }
    VkDescriptorSet Camera::GetDescriptorSet() { return vkCameraDescriptorSet; }

    void Camera::Destroy()
    {
        Mesh* mesh = Mesh::GetRegistry().GetContent(meshID);
        if (mesh) mesh->cameraID = 0;

        Shader* shader = Shader::GetRegistry().GetContent(shaderID);
        if (shader)
        {
            erase(
                shader->cameraIDs, 
                ID);
        }

        GraphicsContext* gctx = GraphicsContext::GetRegistry().GetContent(contextID);
        if (gctx
            && !isDestroyingGraphicsContext)
        {
            erase(
                gctx->cameraIDs, 
                ID);
        }

        registry.RemoveContent(ID);
    }

    Camera::~Camera()
    {
        Log::Print(
            "Destroying camera '" + to_string(ID) + "'.",
            "KG_CAMERA",
            LogType::LOG_INFO);

        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();

        //drain the gpu before destroying this mesh
        if (logicalDevice != VK_NULL_HANDLE) 
        {
            VkResult vkResult = vkDeviceWaitIdle(logicalDevice);
            if (vkResult != VK_SUCCESS)
            {
                GraphicsContext::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to destroy camera '" 
                    + to_string(ID) + "' because vkDeviceWaitIdle did not succeed!",
                    vkResult);
            }
        }

        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();

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

        if (vkCameraDescriptorSet != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(
                logicalDevice,
                GraphicsContext::GetDescriptorPool(),
                1,
                &vkCameraDescriptorSet);

            vkCameraDescriptorSet = VK_NULL_HANDLE;
        }
    }
}