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

#include "resources/kg_mesh.hpp"
#include "core/kg_context.hpp"
#include "resources/kg_shader.hpp"
#include "resources/kg_texture.hpp"
#include "resources/kg_camera.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaMath::mat4;
using KalaHeaders::KalaMath::quat;
using KalaHeaders::KalaMath::createmodelmatrix;
using KalaHeaders::KalaMath::isnear;
using KalaHeaders::KalaMath::normalize_q;
using KalaHeaders::KalaMath::toeuler3;
using KalaHeaders::KalaMath::toquat;
using KalaHeaders::KalaMath::combine3d;

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

        Shader* shader{};
        string err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            Log::Print(
                "Failed to create mesh because the shader was invalid! Reason: " + err,
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

        Texture* texture{};
        err = Texture::GetRegistry().GetContent(textureID, texture);
        if (!err.empty())
        {
            Log::Print(
                "Failed to create mesh because the texture was invalid! Reason: " + err,
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

        meshPtr->isBufferDataDirty = true;
        meshPtr->is2D = shader->Is2D();

        err = registry.AddContent(newID, std::move(newMesh));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics mesh error",
				"Failed to initialize mesh! Reason: " + err);
        }

        Log::Print(
			"Created new mesh '" + to_string(newID) 
            + "' for shader '" + to_string(shaderID) + "'!",
			"KG_MESH",
			LogType::LOG_SUCCESS);

        return meshPtr;
    }

    u32 Mesh::GetID() const { return ID; }
    u32 Mesh::GetHitTestID() const { return hitTestID; }
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

        Shader* oldShader{};
        string err = Shader::GetRegistry().GetContent(shaderID, oldShader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to set shader ID for mesh '" 
                + to_string(ID) + "' because of invalid old shader! Reason: " + err);
        }

        Shader* shader{};
        err = Shader::GetRegistry().GetContent(newValue, shader);
        if (!err.empty())
        {
            Log::Print("Failed to set mesh '" + to_string(ID) 
                + "' shader ID because it was invalid! Reason: " + err,
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (oldShader->viewportID.second
            != shader->viewportID.second)
        {
            Log::Print(
                "Clearing all data for mesh '" + to_string(ID) 
                + "' because new shader '" + to_string(shader->ID) 
                + "' 2D state does not match old shader '" + to_string(oldShader->ID) + "' 2D state!",
                "KG_MESH",
                LogType::LOG_WARNING);

            ClearAllData();
            UpdateMeshData();
        }

        shaderID = newValue;
        is2D = shader->Is2D();

        erase(
            oldShader->meshIDs,
            ID);
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

        Texture* oldTexture{};
        string err = Texture::GetRegistry().GetContent(textureID, oldTexture);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to set texture ID for mesh '" 
                + to_string(ID) + "' because of invalid old texture! Reason: " + err);
        }

        Texture* texture{};
        err = Texture::GetRegistry().GetContent(newValue, texture);
        if (!texture)
        {
            Log::Print("Failed to set mesh '" + to_string(ID) 
                + "' texture ID because it was invalid! Reason: " + err,
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

    u16 Mesh::GetDrawOrderIndex() const { return drawOrderIndex; }
    void Mesh::SetDrawOrderIndex(
        u16 newValue,
        bool sortNow)
    {
        Shader* shader{};
        string err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics viewport error",
                "Failed to set viewport '" + to_string(ID) 
                + "' draw order index because its graphics context was invalid! Reason: " + err);
        }

        if (!is2D)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) 
                + "' draw order index because it is a 3D mesh!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        drawOrderIndex = newValue;

        if (!sortNow) shader->is2DMeshSortDirty = true;
        else shader->Sort2DMeshes();

        Log::Print(
            "Set mesh '" + to_string(ID) + "' draw order index to '" + to_string(drawOrderIndex) + "'.",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    bool Mesh::IsVisible() const { return isVisible; }
    void Mesh::SetVisibleState(bool newValue)
    {
        isVisible = newValue;

        string val = isVisible ? "true" : "false";

        Log::Print(
            "Set mesh '" + to_string(ID) + "' "
            "visible state to " + val + "!", 
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

    bool Mesh::Is2D() const { return is2D; }

    Transform3D& Mesh::GetTransform() { return transform; }

    const vector<Vertex>& Mesh::GetVertices() const { return vertices; }
    void Mesh::SetVertices(vector<Vertex>&& newVertices)
    {
        if (is2D)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) + "' vertices "
                "because user attempted to set 3D vertices to 2D mesh!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (vertices.data() == newVertices.data())
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) + "' vertices "
                "because they already are the same!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

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

        isVertexDataDirty = true;
    }

    const vector<Vertex2D>& Mesh::GetVertices2D() const { return vertices2D; }
    void Mesh::SetVertices2D(vector<Vertex2D>&& newVertices)
    {
        if (!is2D)
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) + "' vertices "
                "because user attempted to set 2D vertices to 3D mesh!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (vertices2D.data() == newVertices.data())
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) + "' vertices "
                "because they already are the same!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

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

        vertices2D = std::move(newVertices);

        isVertexDataDirty = true;
    }

    const vector<u32>& Mesh::GetIndices() const { return indices; }
    void Mesh::SetIndices(vector<u32>&& newIndices)
    {
        if (indices.data() == newIndices.data())
        {
            Log::Print(
                "Failed to set mesh '" + to_string(ID) + "' indices "
                "because they already are the same!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }
    
        indices = std::move(newIndices);

        isIndexDataDirty = true;
    }

    const mat4& Mesh::GetMatrix() const { return meshMatrix; }

    void Mesh::ClearAllData()
    {
        VkDevice logicalDevice = GraphicsContext::GetLogicalDevice();
        if (logicalDevice == VK_NULL_HANDLE)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to clear mesh '" + to_string(ID) 
                + "' data because the logical device was invalid!");
        }

        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();
        if (!allocator)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to clear mesh '" + to_string(ID) 
                + "' data because the vma allocator was invalid!");
        }

        //drain the gpu before destroying this mesh
        VkResult vkResult = vkDeviceWaitIdle(logicalDevice);
        if (vkResult != VK_SUCCESS)
        {
            GraphicsContext::ForceClose(
                "KalaGraphics mesh error",
                "Failed to clear mesh '" 
                + to_string(ID) + "' data because vkDeviceWaitIdle did not succeed!",
                vkResult);
        }

        isBufferDataDirty = false;
        isVertexDataDirty = false;
        isIndexDataDirty = false;

        if (cameraID != 0)
        {
            Camera* cam{};
            string err = Camera::GetRegistry().GetContent(cameraID, cam);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "KalaGraphics mesh error",
                    "Failed to clear mesh '" + to_string(ID) 
                    + "' data because its camera was invalid! Reason: " + err);
            }

            cam->meshID = 0;

            Log::Print(
                "Detached mesh '" + to_string(ID) 
                + "' from camera '" + to_string(cameraID) + "' because mesh data was cleared!",
                "KG_MESH",
                LogType::LOG_WARNING);

            cameraID = 0;
        }

        vertices.clear();
        indices.clear();

        if (vkVertexBuffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(
                allocator,
                vkVertexBuffer,
                vmaVertexAllocation);

            vmaVertexAllocation = VK_NULL_HANDLE;
            vkVertexBuffer = VK_NULL_HANDLE;
            vertexMappedPtr = nullptr;
        }
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

        Shader* shader{};
        string err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to update mesh '" + to_string(ID) 
                + "' data because its shader was invalid! Reason: " + err);
        }

        if (is2D)
        {
            bool hasInvalidValues{};
            
            if (transform.pos_world.z != 0)
            {
                Log::Print(
                    "Transform position Z value for 2D mesh '" + to_string(ID) 
                    + "' must not be anything other than 0! Value was reset to 0.",
                    "KG_MESH",
                    LogType::LOG_WARNING);

                transform.pos_world.z = 0;

                hasInvalidValues = true;
            }

            quat q = normalize_q(transform.rot_world);
            vec3 rot = toeuler3(q);
            if (!isnear(rot.x)
                || !isnear(rot.y))
            {
                Log::Print(
                    "Transform rotation euler angle X or Y value for 2D mesh '" + to_string(ID) 
                    + "' must not be anything other than 0! Values were reset to 0.",
                    "KG_MESH",
                    LogType::LOG_WARNING);

                transform.rot_world = normalize_q(toquat(
                { 
                    0, 
                    0, 
                    rot.z 
                }));

                hasInvalidValues = true;
            }
            
            if (transform.size_world.z != 0)
            {
                Log::Print(
                    "Transform size Z value for 2D mesh '" + to_string(ID) 
                    + "' must not be anything other than 0! Value was reset to 0.",
                    "KG_MESH",
                    LogType::LOG_WARNING);

                transform.size_world.z = 0;

                hasInvalidValues = true;
            }

            if (hasInvalidValues) combine3d(transform, {});
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

        if (isBufferDataDirty)
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

            isBufferDataDirty = false;
        }

        UpdateVertices();
        UpdateIndices();
    }

    void Mesh::UpdateVertices()
    {
        if (!isVertexDataDirty) return;

        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();

        u64 newSize = (is2D 
            ? vertices2D.size() * sizeof(Vertex2D) 
            : vertices.size() * sizeof(Vertex));
        if (newSize == 0)
        {
            Log::Print(
                "Failed to update vertices for mesh '" + to_string(ID) + "' because no vertex data was passed!",
                "KG_MESH",
                LogType::LOG_WARNING);

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
            is2D 
                ? scast<const void*>(vertices2D.data()) 
                : scast<const void*>(vertices.data()), 
            verticesSize);

        isVertexDataDirty = false;
    }

    void Mesh::UpdateIndices()
    {
        if (!isIndexDataDirty) return;

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
        Camera* camera{};
        string err = Camera::GetRegistry().GetContent(cameraID, camera);
        if (err.empty()) camera->meshID = 0;

        //only remove this mesh from texture meshes list of the texture is still valid

        Texture* texture{};
        err = Texture::GetRegistry().GetContent(textureID, texture);
        if (!err.empty())
        {
            erase(
                texture->meshIDs,
                ID);
        }

        //only remove this mesh from shader meshes list if the shader is still valid

        Shader* shader{};
        err = Shader::GetRegistry().GetContent(shaderID, shader);
        if (err.empty())
        {
            erase(
                shader->meshIDs,
                ID);
        }

        err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to destroy mesh '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    Mesh::~Mesh()
    {
        Log::Print(
            "Destroying mesh '" + to_string(ID) + "'.",
            "KG_MESH",
            LogType::LOG_INFO);

        ClearAllData();
    }
}