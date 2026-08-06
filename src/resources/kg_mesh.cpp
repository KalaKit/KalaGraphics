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

    void Mesh::UpdateVertices()
    {
        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();
        if (!allocator)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to initialize vertices for mesh because vma allocator was invalid!");
        }

        size_t bufferSize = vertices.size() * sizeof(Vertex);
        if (bufferSize == 0)
        {
            Log::Print(
                "Failed to create mesh because no vertex data was passed!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
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
                "Failed to create mesh because vertex vk buffer creation failed!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        //upload initial vertex data via pre-mappped pointer
        memcpy(allocResult.pMappedData, vertices.data(), bufferSize);

        vkVertexBuffer = vkBuffer;
        vmaVertexAllocation = vmaAllocation;
        vertexBufferSize = bufferSize;
        vertexMappedPtr = allocResult.pMappedData;

        SyncToGPU();
    }

    void Mesh::UpdateIndices()
    {
        VmaAllocator allocator = GraphicsContext::GetVmaAllocator();
        if (!allocator)
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics mesh error",
                "Failed to initialize indices for mesh because vma allocator was invalid!");
        }

        size_t bufferSize = indices.size() * sizeof(u32);

        //empty indices = non-indexed mesh, not an error
        if (bufferSize == 0)
        {
            vkIndexBuffer = VK_NULL_HANDLE;
            vmaIndexAllocation = VK_NULL_HANDLE;
            bufferSize = 0;
            indexMappedPtr = nullptr;

            return;
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
                "Failed to create mesh because index vk buffer creation failed!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        memcpy(allocResult.pMappedData, indices.data(), bufferSize);

        vkIndexBuffer = vkBuffer;
        vmaIndexAllocation = vmaAllocation;
        indexBufferSize = bufferSize;
        indexMappedPtr = allocResult.pMappedData;

        SyncToGPU();
    }

    void Mesh::SyncToGPU()
    {
        if (vertexMappedPtr)
        {
            size_t bufferSize = vertices.size() * sizeof(Vertex);
            if (bufferSize > 0
                && bufferSize == vertexBufferSize)
            {
                memcpy(vertexMappedPtr, vertices.data(), bufferSize);
            }
            else
            {
                Log::Print("@@@@@ vertex buffer for mesh '" + to_string(ID) + "' out of size!");
            }
        }
        if (indexMappedPtr)
        {
            size_t bufferSize = indices.size() * sizeof(u32);
            if (bufferSize > 0
                && bufferSize == indexBufferSize)
            {
                memcpy(indexMappedPtr, indices.data(), bufferSize);
            }
            else
            {
                Log::Print("@@@@@ index buffer for mesh '" + to_string(ID) + "' out of size!");
            }
        }
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

    VkBuffer& Mesh::GetVkBuffer(bool vertex) 
    { 
        return vertex 
            ? vkVertexBuffer
            : vkIndexBuffer;
    }

    vector<Vertex>& Mesh::GetVertices() { return vertices; }
    vector<u32>& Mesh::GetIndices() { return indices; }

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

        VkDevice device = GraphicsContext::GetLogicalDevice();

        //drain the gpu before destroying this mesh
        if (device != VK_NULL_HANDLE) 
        {
            VkResult vkResult = vkDeviceWaitIdle(device);
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
    }
}