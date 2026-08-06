//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "vulkan/vulkan_core.h"
#include "vma/vk_mem_alloc.h"

#include "log_utils.hpp"

#include "resources/kg_mesh.hpp"
#include "core/kg_core.hpp"
#include "core/kg_context.hpp"
#include "resources/kg_shader.hpp"
#include "resources/kg_camera.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::mat4;
using KalaHeaders::KalaMath::createumodel;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;

using std::to_string;
using std::unique_ptr;
using std::make_unique;

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Mesh> registry{};

    KalaGraphicsRegistry<Mesh>& Mesh::GetRegistry() { return registry; }

    Mesh* Mesh::Initialize(u32 shaderID)
    {
        Shader* shader = Shader::GetRegistry().GetContent(shaderID);
        if (!shader)
        {
            Log::Print(
                "Failed to create mesh because shader '" + to_string(shaderID) + "' was invalid!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<Mesh> newMesh = make_unique<Mesh>();
        Mesh* meshPtr = newMesh.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        meshPtr->ID = newID;
        meshPtr->shaderID = shaderID;

        //shader references this mesh
        shader->meshIDs.push_back(newID);

        registry.AddContent(newID, std::move(newMesh));

        Log::Print(
			"Created new mesh '" + to_string(newID) 
            + "' for shader '" + to_string(shaderID) + "'!",
			"KG_MESH",
			LogType::LOG_SUCCESS);

        return meshPtr;
    }

    u32 Mesh::GetID() const { return ID; }

    u32 Mesh::GetCameraID() const { return cameraID; }

    u32 Mesh::GetShaderID() const { return shaderID; }
    void Mesh::SetShaderID(u32 newValue)
    {
        if (shaderID == newValue)
        {
            Log::Print("Failed to set mesh '" + to_string(ID) 
                + "' shader ID to '" + to_string(newValue) 
                + "' because it already is that value!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        Shader* oldShader = Shader::GetRegistry().GetContent(shaderID);
        Shader* shader = Shader::GetRegistry().GetContent(newValue);
        if (!shader)
        {
            Log::Print("Failed to set mesh '" + to_string(ID) 
                + "' shader ID to '" + to_string(newValue) 
                + "' because it was invalid!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        shaderID = newValue;

        if (oldShader)
        {
            erase(
                oldShader->meshIDs,
                ID);
        }
        shader->meshIDs.push_back(ID);

        Log::Print(
            "Set mesh '" + to_string(ID) 
            + "' shader ID to '" + to_string(shaderID) + "'!",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    void Mesh::UpdateMeshData()
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to update mesh '" + to_string(ID) 
                + "' vertices because the logical device was invalid!");
        }

        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();
        if (!allocator)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to update mesh data for mesh '" + to_string(ID) + "' because vma allocator was invalid!");
        }

        Shader* shader = Shader::GetRegistry().GetContent(shaderID);
        if (!shader)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to update mesh '" + to_string(ID) 
                + "' because its shader '" + to_string(shaderID) + "' was invalid!");
        }

        if (vkDescriptorSet == VK_NULL_HANDLE)
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
                    "Failed to update mesh data for mesh '" + to_string(ID) 
                    + "' and shader '" + to_string(shaderID) + "' because descriptor set init failed! Reason: " 
                    + GraphicsContext::GetVkResultMessage(vkResult));
            }

            vkDescriptorSet = newDescriptorSet;
        }

        //create new transform data if it doesnt exist yet
        if (vkTransformUBOBuffer == VK_NULL_HANDLE)
        {
            size_t bufferSize = sizeof(mat4);

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = bufferSize;
            bufferInfo.usage = 
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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
                    "Failed to update mesh data for mesh '" + to_string(ID) 
                    + "' because transform UBO vk buffer creation failed!",
                    "KG_MESH",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            //upload initial vertex data via pre-mappped pointer
            memcpy(allocResult.pMappedData, vertices.data(), bufferSize);

            vkTransformUBOBuffer = vkBuffer;
            vmaTransformUBOAllocation = vmaAllocation;
            transformUBOMappedPtr = allocResult.pMappedData;
        }

        //destroy unused camera data
        if (cameraID == 0
            && vkCameraUBOBuffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(
                allocator,
                vkCameraUBOBuffer,
                vmaCameraUBOAllocation);

            vkCameraUBOBuffer = VK_NULL_HANDLE;
            vmaCameraUBOAllocation = VK_NULL_HANDLE;
            cameraUBOMappedPtr = nullptr;
        }
        //create new camera data if it doesnt exist yet
        if (cameraID != 0
            && vkCameraUBOBuffer == VK_NULL_HANDLE)
        {
            size_t bufferSize = sizeof(mat4);

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = bufferSize;
            bufferInfo.usage = 
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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
                    "Failed to update mesh data for mesh '" + to_string(ID) 
                    + "' because transform UBO vk buffer creation failed!",
                    "KG_MESH",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            //upload initial vertex data via pre-mappped pointer
            memcpy(allocResult.pMappedData, vertices.data(), bufferSize);

            vkCameraUBOBuffer = vkBuffer;
            vmaCameraUBOAllocation = vmaAllocation;
            cameraUBOMappedPtr = allocResult.pMappedData;
        }

        mat4 modelMatrix = createumodel(
            50.0f, 
            {}, 
            100.0f);
        memcpy(
            transformUBOMappedPtr, 
            &modelMatrix, 
            sizeof(mat4));

        VkDescriptorBufferInfo transformInfo{};
        transformInfo.buffer = vkTransformUBOBuffer;
        transformInfo.offset = 0;
        transformInfo.range = sizeof(mat4);

        //TODO: add camera info and write for camera too

        VkWriteDescriptorSet writes[1]{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = vkDescriptorSet;
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

        UpdateVertices();
        UpdateIndices();
    }

    bool Mesh::Is2D() const { return is2D; }
    void Mesh::Set2DState(bool newState)
    {
        if (newState)
        {
            if (transform.pos_world.z != 0
                || transform.rot_world.y != 0
                || transform.rot_world.z != 0
                || transform.size_world.z != 0)
            {
                Log::Print(
                        "Failed to set mesh 2D state because 2D state was requested "
                        "but 3D values were assigned to transform pos, rot or size!",
                    "KG_MESH",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            for (const auto& v : vertices)
            {
                if (v.pos.z != 0)
                {
                    Log::Print(
                        "Failed to set mesh 2D state because 2D state was requested "
                        "but 3D values were assigned to one of the vertice positions!",
                        "KG_MESH",
                        LogType::LOG_ERROR,
                        2);

                    return;
                }
            }
        }

        is2D = newState;
    }

    Transform3D& Mesh::GetTransform() { return transform; }

    vector<Vertex>& Mesh::GetVertices() { return vertices; }
    vector<u32>& Mesh::GetIndices() { return indices; }

    VkDescriptorSet Mesh::GetVkDescriptorSet() { return vkDescriptorSet; }

    void Mesh::UpdateVertices()
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();

        size_t bufferSize = vertices.size() * sizeof(Vertex);
        if (bufferSize == 0)
        {
            Log::Print(
                "Failed to update vertices for mesh '" + to_string(ID) + "' because no vertex data was passed!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (bufferSize != vertexBufferSize
            || vkVertexBuffer == VK_NULL_HANDLE)
        {
            if (vkVertexBuffer != VK_NULL_HANDLE)
            {
                Log::Print(
                    "Recreating vertex buffer during mesh '" + to_string(ID) 
                    + "' update because old buffer size did not match new buffer size.",
                    "KG_MESH",
                    LogType::LOG_INFO);

                vmaDestroyBuffer(
                    allocator,
                    vkVertexBuffer,
                    vmaVertexAllocation);

                vmaVertexAllocation = VK_NULL_HANDLE;
                vkVertexBuffer = VK_NULL_HANDLE;
                vertexMappedPtr = nullptr;
            }

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = bufferSize;
            bufferInfo.usage = 
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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
                    "Failed to update vertices for mesh '" + to_string(ID) + "' because vertex vk buffer creation failed!",
                    "KG_MESH",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            vkVertexBuffer = vkBuffer;
            vmaVertexAllocation = vmaAllocation;
            vertexBufferSize = bufferSize;
            vertexMappedPtr = allocResult.pMappedData;
        }

        memcpy(
            vertexMappedPtr, 
            vertices.data(), 
            bufferSize);
    }

    void Mesh::UpdateIndices()
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();

        size_t bufferSize = indices.size() * sizeof(u32);

        //empty indices = non-indexed mesh, not an error
        if (bufferSize == 0)
        {
            if (vkIndexBuffer != VK_NULL_HANDLE)
            {
                vmaDestroyBuffer(
                    allocator,
                    vkIndexBuffer,
                    vmaIndexAllocation);

                vmaIndexAllocation = VK_NULL_HANDLE;
                vkIndexBuffer = VK_NULL_HANDLE;
                indexMappedPtr = nullptr;
            }

            return;
        }

        if (indexBufferSize != bufferSize
            || vkIndexBuffer == VK_NULL_HANDLE)
        {
            if (vkIndexBuffer != VK_NULL_HANDLE)
            {
                Log::Print(
                    "Recreating index buffer during mesh '" + to_string(ID) 
                    + "' update because old buffer size did not match new buffer size.",
                    "KG_MESH",
                    LogType::LOG_INFO);

                vmaDestroyBuffer(
                    allocator,
                    vkIndexBuffer,
                    vmaIndexAllocation);

                vmaIndexAllocation = VK_NULL_HANDLE;
                vkIndexBuffer = VK_NULL_HANDLE;
                indexMappedPtr = nullptr;
            }

            VkBufferCreateInfo indexBufferInfo{};
            indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            indexBufferInfo.size = bufferSize;
            indexBufferInfo.usage =
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo indexAllocInfo{};
            indexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            indexAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            indexAllocInfo.flags = 
                VMA_ALLOCATION_CREATE_MAPPED_BIT
                | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

            VkBuffer vkBuffer = VK_NULL_HANDLE;
            VmaAllocation vmaAllocation = VK_NULL_HANDLE;
            VmaAllocationInfo allocResult{};

            VkResult result = vmaCreateBuffer(
                allocator,
                &indexBufferInfo,
                &indexAllocInfo,
                &vkBuffer,
                &vmaAllocation,
                &allocResult);

            if (result != VK_SUCCESS)
            {
                Log::Print(
                    "Failed to update indices for mesh '" + to_string(ID) + "' because index vk buffer creation failed!",
                    "KG_MESH",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            vkIndexBuffer = vkBuffer;
            vmaIndexAllocation = vmaAllocation;
            indexBufferSize = bufferSize;
            indexMappedPtr = allocResult.pMappedData;
        }

        memcpy(
            indexMappedPtr, 
            indices.data(), 
            indexBufferSize);
    }

    void Mesh::Destroy()
    {
        Camera* camera = Camera::GetRegistry().GetContent(cameraID);
        if (camera) camera->meshID = 0;

        //only remove this mesh from shader meshes list if the shader is still valid

        Shader* shader = Shader::GetRegistry().GetContent(shaderID);
        if (shader)
        {
            erase(
                shader->meshIDs,
                ID);
        }

        registry.RemoveContent(ID);
    }

    Mesh::~Mesh()
    {
        Log::Print(
            "Destroying mesh '" + to_string(ID) + "'.",
            "KG_MESH",
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
                    "Failed to destroy mesh '" 
                    + to_string(ID) + "' because vkDeviceWaitIdle did not succeed!",
                    vkResult);
            }
        }

        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();

        if (vmaVertexAllocation)
        {
            vmaDestroyBuffer(
                allocator,
                vkVertexBuffer,
                vmaVertexAllocation);

            vmaVertexAllocation = VK_NULL_HANDLE;
            vkVertexBuffer = VK_NULL_HANDLE;
            vertexMappedPtr = nullptr;
        }
        if (vmaIndexAllocation)
        {
            vmaDestroyBuffer(
                allocator,
                vkIndexBuffer,
                vmaIndexAllocation);

            vmaIndexAllocation = VK_NULL_HANDLE;
            vkIndexBuffer = VK_NULL_HANDLE;
            indexMappedPtr = nullptr;
        }

        if (vmaTransformUBOAllocation)
        {
            vmaDestroyBuffer(
                allocator,
                vkTransformUBOBuffer,
                vmaTransformUBOAllocation);

            vmaTransformUBOAllocation = VK_NULL_HANDLE;
            vkTransformUBOBuffer = VK_NULL_HANDLE;
            transformUBOMappedPtr = nullptr;
        }
        if (vmaCameraUBOAllocation)
        {
            vmaDestroyBuffer(
                allocator,
                vkCameraUBOBuffer,
                vmaCameraUBOAllocation);

            vmaCameraUBOAllocation = VK_NULL_HANDLE;
            vkCameraUBOBuffer = VK_NULL_HANDLE;
            cameraUBOMappedPtr = nullptr;
        }

        if (vkDescriptorSet != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(
                logicalDevice,
                GraphicsContext::GetDescriptorPool(),
                1,
                &vkDescriptorSet);

            vkDescriptorSet = VK_NULL_HANDLE;
        }

        //TODO: destroy transform UBO?
    }
}