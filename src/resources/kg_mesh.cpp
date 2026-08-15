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
#include "resources/kg_texture.hpp"
#include "resources/kg_camera.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::mat4;
using KalaHeaders::KalaMath::createmodelmatrix;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Core::GraphicsContext;

using std::to_string;
using std::unique_ptr;
using std::make_unique;

namespace KalaGraphics::Resources
{
    static KalaGraphicsRegistry<Mesh> registry{};

    KalaGraphicsRegistry<Mesh>& Mesh::GetRegistry() { return registry; }

    Mesh* Mesh::Initialize(
        u32 shaderID,
        u32 textureID)
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to create mesh because the logical device was invalid!");
        }

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

        //TODO: figure out a better solution
        if (shader->descriptorSetLayouts.empty())
        {
            Log::Print(
                "Failed to create mesh because the shader '" 
                + to_string(shaderID) + "' had no shader data!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        Texture* texture = Texture::GetRegistry().GetContent(textureID);
        if (!texture)
        {
            Log::Print(
                "Failed to create mesh because texture '" + to_string(textureID) + "' was invalid!",
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
        meshPtr->textureID = textureID;

        //texture references this mesh
        texture->meshIDs.push_back(newID);

        //shader references this mesh
        shader->meshIDs.push_back(newID);

        //always assign descriptor set data at mesh init
        meshPtr->isDirty = true;

        meshPtr->UpdateMeshData();

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
        //TODO: figure out if changing shader messes up camera and texture

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

    u32 Mesh::GetTextureID() const { return textureID; }
    void Mesh::SetTextureID(u32 newValue)
    {
        if (textureID == newValue)
        {
            Log::Print("Failed to set mesh '" + to_string(ID) 
                + "' texture ID to '" + to_string(newValue) 
                + "' because it already is that value!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        Texture* oldTexture = Texture::GetRegistry().GetContent(textureID);
        Texture* texture = Texture::GetRegistry().GetContent(newValue);
        if (!texture)
        {
            Log::Print("Failed to set mesh '" + to_string(ID) 
                + "' texture ID to '" + to_string(newValue) 
                + "' because it was invalid!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        textureID = newValue;

        if (oldTexture)
        {
            erase(
                oldTexture->meshIDs,
                ID);
        }
        texture->meshIDs.push_back(ID);

        Log::Print(
            "Set mesh '" + to_string(ID) 
            + "' texture ID to '" + to_string(textureID) + "'!",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    const Transform3D& Mesh::GetTransform() const { return transform; }
    void Mesh::SetTransform(Transform3D&& newTransform)
    {
        //TODO: figure out if transform safety checks are even needed
        transform = std::move(newTransform);
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
                        "Failed to set mesh '" + to_string(ID) + "' 2D state because 2D state was requested "
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
                        "Failed to set mesh '" + to_string(ID) + "' 2D state because 2D state was requested "
                        "but 3D values were assigned to one of the vertice positions!",
                        "KG_MESH",
                        LogType::LOG_ERROR,
                        2);

                    return;
                }
            }
        }

        is2D = newState;

        string state = is2D ? "true" : "false";

        Log::Print(
            "Set mesh '" + to_string(ID) + "' 2D state to '" + state + "'.",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    const vector<Vertex>& Mesh::GetVertices() const { return vertices; }
    void Mesh::SetVertices(vector<Vertex>&& newVertices)
    {
        size_t bufferSize = newVertices.size() * sizeof(Vertex);
        if (bufferSize == 0)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) + "' vertices "
                "because no vertex data was passed!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        vertices = std::move(newVertices);

        UpdateVertices();
    }

    const vector<u32>& Mesh::GetIndices() const { return indices; }
    void Mesh::SetIndices(vector<u32>&& newIndices)
    {
        //TODO: figure out if index safety checks are even needed
        indices = std::move(newIndices);

        UpdateIndices();
    }

    const mat4& Mesh::GetMatrix() const { return meshMatrix; }

    void Mesh::UpdateMeshData()
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to update mesh '" + to_string(ID) 
                + "' data because the logical device was invalid!");
        }

        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();
        if (!allocator)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to update mesh '" + to_string(ID) + "' data "
                "because the vma allocator was invalid!");
        }

        Shader* shader = Shader::GetRegistry().GetContent(shaderID);
        if (!shader)
        {
            Log::Print(
                "Failed to update mesh '" + to_string(ID) 
                + "' data because the shader '" + to_string(shaderID) + "' was invalid!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
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
            descriptorSetAllocateInfo.pSetLayouts = &shader->descriptorSetLayouts[1];

            VkDescriptorSet newDescriptorSet;
            VkResult vkResult = vkAllocateDescriptorSets(
                logicalDevice,
                &descriptorSetAllocateInfo,
                &newDescriptorSet);

            if (vkResult != VK_SUCCESS)
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh update error",
                    "Failed to update mesh because descriptor set init failed! Reason: " 
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
                    "KalaGraphics mesh update error",
                    "Failed to update mesh because vma allocator init failed! Reason: " 
                    + GraphicsContext::GetVkResultMessage(vkResult));
            }

            vkMeshUBOBuffer = newBuffer;
            vmaMeshUBOAllocation = newAllocation;
            meshUBOMappedPtr = allocResult.pMappedData;
        }

        meshMatrix = createmodelmatrix(
            transform.pos_world, 
            transform.rot_world, 
            transform.size_world);

        memcpy(
            meshUBOMappedPtr,
            &meshMatrix,
            sizeof(mat4));

        if (isDirty)
        {
            VkDescriptorBufferInfo transformInfo{};
            transformInfo.buffer = vkMeshUBOBuffer;
            transformInfo.offset = 0;
            transformInfo.range = sizeof(mat4);

            VkWriteDescriptorSet writes[1]{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = vkDescriptorSet;
            writes[0].dstBinding = 0; // <<<< SET 1 BINDING 0 - MESH UBO SLOT
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

        UpdateVertices();
        UpdateIndices();

        Log::Print(
            "Updated mesh '" + to_string(ID) + "' data!",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    void Mesh::UpdateVertices()
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();

        u64 newSize = vertices.size() * sizeof(Vertex);
        if (newSize == 0)
        {
            Log::Print(
                "Failed to update vertices for mesh '" + to_string(ID) + "' because no vertex data was passed!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (newSize != verticesSize
            || vkVertexBuffer == VK_NULL_HANDLE)
        {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = newSize;
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
                Log::Print(
                    "Failed to update vertices for mesh '" + to_string(ID) + "' because vertex vk buffer creation failed!",
                    "KG_MESH",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

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

            vkVertexBuffer = newBuffer;
            verticesSize = newSize;
            vmaVertexAllocation = newAllocation;
            vertexMappedPtr = allocResult.pMappedData;
        }

        memcpy(
            vertexMappedPtr, 
            vertices.data(), 
            verticesSize);
    }

    void Mesh::UpdateIndices()
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();

        u64 newSize = indices.size() * sizeof(u32);

        //empty indices = non-indexed mesh, not an error
        if (newSize == 0)
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

        if (indicesSize != newSize
            || vkIndexBuffer == VK_NULL_HANDLE)
        {
            VkBufferCreateInfo indexBufferInfo{};
            indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            indexBufferInfo.size = newSize;
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

            VkBuffer newBuffer{};
            VmaAllocation newAllocation{};
            VmaAllocationInfo allocResult{};

            VkResult result = vmaCreateBuffer(
                allocator,
                &indexBufferInfo,
                &indexAllocInfo,
                &newBuffer,
                &newAllocation,
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

            vkIndexBuffer = newBuffer;
            indicesSize = newSize;
            vmaIndexAllocation = newAllocation;
            indexMappedPtr = allocResult.pMappedData;
        }

        memcpy(
            indexMappedPtr, 
            indices.data(), 
            indicesSize);
    }

    void Mesh::Destroy()
    {
        Camera* camera = Camera::GetRegistry().GetContent(cameraID);
        if (camera) camera->meshID = 0;

        //only remove this mesh from texture meshes list of the texture is still valid

        Texture* texture = Texture::GetRegistry().GetContent(textureID);
        if (texture)
        {
            erase(
                texture->meshIDs,
                ID);
        }

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

            vertexMappedPtr = nullptr;
        }
        if (vmaIndexAllocation)
        {
            vmaDestroyBuffer(
                allocator,
                vkIndexBuffer,
                vmaIndexAllocation);

            indexMappedPtr = nullptr;
        }

        if (vmaMeshUBOAllocation != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(
                allocator,
                vkMeshUBOBuffer,
                vmaMeshUBOAllocation);

            meshUBOMappedPtr = nullptr;
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
}