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

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaMath::toquat;

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
        bool use2D,
        u32 contextID,
        u32 shaderID,
        Transform&& transform,
        vector<Vertex>&& vertices,
        vector<u32>&& indices)
    {
        GraphicsContext* gctx = GraphicsContext::GetRegistry().GetContent(contextID);
        if (!gctx)
        {
            Log::Print(
                "Failed to create mesh because graphics context '" + to_string(contextID) + "' was invalid!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
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

        if (use2D)
        {
            if (transform.pos.z != 0
                || transform.rot.y != 0
                || transform.rot.z != 0
                || transform.size.z != 0)
            {
                Log::Print(
                    "Failed to create mesh because user requested 2D "
                    "but assigned 3D values to transform!",
                    "KG_MESH",
                    LogType::LOG_ERROR,
                    2);

                return nullptr;
            }

            for (const auto& v : vertices)
            {
                if (v.pos.z != 0)
                {
                    Log::Print(
                        "Failed to create mesh because user requested 2D "
                        "but assigned 3D values to one of the vertice positions!",
                        "KG_MESH",
                        LogType::LOG_ERROR,
                        2);

                    return nullptr;
                }
            }
        }

        unique_ptr<Mesh> newMesh = make_unique<Mesh>();
        Mesh* meshPtr = newMesh.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        meshPtr->ID = newID;
        meshPtr->shaderID = shaderID;
        shader->contextID = contextID;

        meshPtr->is2D = use2D;

        meshPtr->transform.pos_world = transform.pos;
        meshPtr->transform.rot_world = toquat(transform.rot);
        meshPtr->transform.size_world = transform.size;

        meshPtr->vertices = std::move(vertices);
        meshPtr->indices = std::move(indices);

        if (!meshPtr->InitVertices())
        {
            return nullptr;
        }
        if (!meshPtr->InitIndices())
        {
            return nullptr;
        }

        meshPtr->SyncToGPU();

        //shader references this mesh
        shader->meshIDs.push_back(newID);

        //graphics context references this mesh
        gctx->meshIDs.push_back(newID);

        registry.AddContent(newID, std::move(newMesh));

        Log::Print(
			"Created new mesh '" + to_string(newID) 
            + "' for shader '" + to_string(shaderID) +
            + "' and graphics context '" + to_string(contextID) + "'!",
			"KG_MESH",
			LogType::LOG_SUCCESS);

        return meshPtr;
    }

    bool Mesh::InitVertices()
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

            return false;
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

            return false;
        }

        //upload initial vertex data via pre-mappped pointer
        memcpy(allocResult.pMappedData, vertices.data(), bufferSize);

        vkVertexBuffer = vkBuffer;
        vmaVertexAllocation = vmaAllocation;
        vertexBufferSize = bufferSize;
        vertexMappedPtr = allocResult.pMappedData;

        return true;
    }

    bool Mesh::InitIndices()
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

            return true;
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

            return false;
        }

        memcpy(allocResult.pMappedData, indices.data(), bufferSize);

        vkIndexBuffer = vkBuffer;
        vmaIndexAllocation = vmaAllocation;
        indexBufferSize = bufferSize;
        indexMappedPtr = allocResult.pMappedData;

        return true;
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

    u32 Mesh::GetContextID() const { return contextID; }
    void Mesh::SetContextID(u32 newValue)
    {
        if (contextID == newValue)
        {
            Log::Print("Failed to set mesh '" + to_string(ID) 
                + "' graphics context ID to '" + to_string(newValue) 
                + "' because it already is that value!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        GraphicsContext* oldGctx = GraphicsContext::GetRegistry().GetContent(contextID);
        GraphicsContext* gctx = GraphicsContext::GetRegistry().GetContent(newValue);
        if (!gctx)
        {
            Log::Print("Failed to set mesh '" + to_string(ID) 
                + "' graphics context ID to '" + to_string(newValue) 
                + "' because it was invalid!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        Shader* shader = Shader::GetRegistry().GetContent(shaderID);
        if (shader
            && shader->contextID != newValue)
        {
            u32 oldShaderID = shaderID;

            shaderID = 0;
            erase(
                shader->meshIDs,
                ID);

            Log::Print("Removed shader '" + to_string(oldShaderID) 
                + "' from mesh '" + to_string(ID) 
                + "' because their graphics context IDs no longer match.",
                "KG_MESH",
                LogType::LOG_WARNING);
        }

        contextID = newValue;

        erase(
            oldGctx->meshIDs,
            ID);
        gctx->meshIDs.push_back(ID);

        Log::Print(
            "Set mesh '" + to_string(ID) 
            + "' graphics context ID to '" + to_string(contextID) + "'!",
            "KG_MESH",
            LogType::LOG_SUCCESS);
    }

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

        if (shader->contextID != contextID)
        {
            Log::Print("Failed to set mesh '" + to_string(ID) 
                + "' shader ID to '" + to_string(newValue) 
                + "' because their graphics context IDs don't match!",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);

            return;
        }

        shaderID = newValue;

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

    bool Mesh::Is2D() const { return is2D; }

    Transform3D& Mesh::GetTransform() { return transform; }

    VkBuffer& Mesh::GetVkBuffer(bool vertex) 
    { 
        return vertex 
            ? vkVertexBuffer
            : vkIndexBuffer;
    }

    void Mesh::Destroy()
    {
        GraphicsContext* gctx = GraphicsContext::GetRegistry().GetContent(contextID);
        if (gctx
            && !isDestroyingGraphicsContext)
        {
            erase(
                gctx->meshIDs, 
                ID);
        }

        //only remove this mesh from shader meshes list if the shader is still valid
        if (shaderID != 0)
        {
            Shader* s = Shader::GetRegistry().GetContent(shaderID);
            if (s)
            {
                erase(
                    s->meshIDs,
                    ID);
            }
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