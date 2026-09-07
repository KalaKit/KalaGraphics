//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "cgltf.h"

#include "log_utils.hpp"
#include "file_utils.hpp"

#include "import/kg_import_mesh.hpp"
#include "import/kg_import_texture.hpp"
#include "core/kg_core.hpp"
#include "graphics/kg_mesh.hpp"
#include "graphics/kg_texture.hpp"
#include "graphics/kg_shader.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaFile::ReadBinaryDataFromFile;

using KalaHeaders::KalaMath::PosTarget;
using KalaHeaders::KalaMath::RotTarget;
using KalaHeaders::KalaMath::SizeTarget;
using KalaHeaders::KalaMath::Transform3D;
using KalaHeaders::KalaMath::mat4;
using KalaHeaders::KalaMath::vec4;
using KalaHeaders::KalaMath::vec3;
using KalaHeaders::KalaMath::vec2;
using KalaHeaders::KalaMath::topos;
using KalaHeaders::KalaMath::toquat;
using KalaHeaders::KalaMath::tosize;

using KalaGraphics::Core::KalaGraphicsCore;
using KalaGraphics::Graphics::Vertex;
using KalaGraphics::Graphics::AlphaMode;
using KalaGraphics::Graphics::Texture;
using KalaGraphics::Graphics::Shader;
using KalaGraphics::Import::ImportNodeData;
using KalaGraphics::Import::ImportPrimitiveData;
using KalaGraphics::Import::ImportMeshData;
using KalaGraphics::Import::ImportMaterialData;
using KalaGraphics::Import::ImportTextureData;
using KalaGraphics::Import::ImportTexture;

using std::string;
using std::string_view;
using std::to_string;
using std::vector;
using std::unique_ptr;
using std::pair;
using std::make_unique;
using std::filesystem::path;
using std::filesystem::is_regular_file;
using std::filesystem::absolute;

static constexpr string_view EXT_GLTF = ".gltf";
static constexpr string_view EXT_GLB = ".glb";

static string CGLTFErrorToString(cgltf_result result)
{
    switch (result)
    {
    default: return "Unknown result";
    case cgltf_result_max_enum: return "Invalid result";

    case cgltf_result_success: return "Success";

    case cgltf_result_data_too_short:  return "Data is too short";
    case cgltf_result_unknown_format:  return "Format is not recognized";
    case cgltf_result_invalid_json:    return "The parsed json data is invalid";
    case cgltf_result_invalid_gltf:    return "The parsed gltf data is invalid";
    case cgltf_result_invalid_options: return "CGLTF options are invalid";
    case cgltf_result_file_not_found:  return "The import file was not found";
    case cgltf_result_io_error:        return "Encountered an IO error";
    case cgltf_result_out_of_memory:   return "Failed to allocate enough system memory";
    case cgltf_result_legacy_gltf:     return "Unsupported gltf/glb version";
    }
};

static string Init_GLTF_GLB(
    const path& meshPath,
    vector<u8>&& binaryData,
    vector<ImportNodeData>& outNodeData);

static bool GetPixelData(
    const u32 nodeIndex,
    const string& nodeName,
    const string& materialName,
    cgltf_texture& texture,
    ImportTextureData& texData);

namespace KalaGraphics::Import
{
    static KalaGraphicsRegistry<ImportMesh> registry{};

    KalaGraphicsRegistry<ImportMesh>& ImportMesh::GetRegistry() { return registry; }

    ImportMesh* ImportMesh::Initialize(path&& meshPath)
    {
        if (!exists(meshPath))
        {
            Log::Print(
                "Failed to import mesh '" + meshPath.string() + "' because it was not found!",
                "KG_IMPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        if (!is_regular_file(meshPath))
        {
            Log::Print(
                "Failed to import mesh '" + meshPath.string() + "' because it is not a regular file!",
                "KG_IMPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        string ext = meshPath.extension().string();
        if (ext != EXT_GLTF
            && ext != EXT_GLB)
        {
            Log::Print(
                "Failed to import mesh '" + meshPath.string() + "' because its extension is not supported!",
                "KG_IMPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        vector<u8> outData{};
        string errMsg = ReadBinaryDataFromFile(
            meshPath,
            outData);

        if (!errMsg.empty())
        {
            Log::Print(
                "Failed to import mesh '" + meshPath.string() + "'! Reason: " + errMsg,
                "KG_IMPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        vector<ImportNodeData> nodeData{};
        if (ext == EXT_GLTF
            || ext == EXT_GLB)
        {
            errMsg = Init_GLTF_GLB(
                absolute(meshPath),
                std::move(outData),
                nodeData);
        }

        if (!errMsg.empty())
        {
            Log::Print(
                "Failed to import mesh '" + meshPath.string() + "'! Reason: " + errMsg,
                "KG_IMPORT_MESH",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<ImportMesh> newMesh = make_unique<ImportMesh>();
        ImportMesh* meshPtr = newMesh.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        meshPtr->ID = newID;
        meshPtr->meshPath = std::move(meshPath);
        meshPtr->nodeData = std::move(nodeData);

        /*
        for (const ImportNodeData& thisNodeData : meshPtr->nodeData)
        {
            const Transform3D& mt = thisNodeData.transform;

            Log::Print(
                "@@@@@\n"
                "IMPORT TRANSFORM\n"
                "pos: "
                + to_string(mt.getpos(PosTarget::POS_LOCAL).x) + ", "
                + to_string(mt.getpos(PosTarget::POS_LOCAL).y) + ", "
                + to_string(mt.getpos(PosTarget::POS_LOCAL).z) + "\n"
                "rot: "
                + to_string(mt.getrotquat(RotTarget::ROT_LOCAL).w) + ", "
                + to_string(mt.getrotquat(RotTarget::ROT_LOCAL).x) + ", "
                + to_string(mt.getrotquat(RotTarget::ROT_LOCAL).y) + ", "
                + to_string(mt.getrotquat(RotTarget::ROT_LOCAL).z) + "\n"
                "size: "
                + to_string(mt.getsize(SizeTarget::SIZE_LOCAL).x) + ", "
                + to_string(mt.getsize(SizeTarget::SIZE_LOCAL).y) + ", "
                + to_string(mt.getsize(SizeTarget::SIZE_LOCAL).z));
        }
        */

        string err = registry.AddContent(newID, std::move(newMesh));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics import mesh error",
				"Failed to initialize import mesh! Reason: " + err);
        }

        Log::Print(
			"Created new import mesh '" + to_string(newID) 
            + "' from path '" + meshPtr->meshPath.string() + "'!",
			"KG_IMPORT_MESH",
			LogType::LOG_SUCCESS);

        return meshPtr;
    }

    u32 ImportMesh::GetID() const { return ID; }

    const path& ImportMesh::GetMeshPath() const { return meshPath; }
    const vector<ImportNodeData>& ImportMesh::GetMeshData() const { return nodeData; }

    void ImportMesh::Destroy()
    {
        string err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics import mesh error",
                "Failed to destroy import mesh '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    ImportMesh::~ImportMesh()
    {
        Log::Print(
            "Destroying import mesh data '" + to_string(ID) + "'.",
            "KG_IMPORT_MESH",
            LogType::LOG_INFO);
    }
}

string Init_GLTF_GLB(
    const path& meshPath,
    vector<u8>&& binaryData,
    vector<ImportNodeData>& outNodeData)
{
    cgltf_options options{};
    cgltf_data* data{};

    cgltf_result result = cgltf_parse(
        &options,
        binaryData.data(),
        binaryData.size(),
        &data);

    if (result != cgltf_result_success)
    {
        return "Failed to parse GLTF/GLB data! Reason: " + CGLTFErrorToString(result);
    }

    result = cgltf_load_buffers(
        &options,
        data,
        meshPath.string().c_str());

    if (result != cgltf_result_success)
    {
        cgltf_free(data);

        return "Failed to load GLTF/GLB buffers! Reason: " + CGLTFErrorToString(result);
    }

    /*
    for (cgltf_size i = 0; i < data->buffers_count; ++i)
    {
        Log::Print(
            "@@@@@\n"
            "buffer: " + to_string(i) + "\n"
            "buffer size: " + to_string(data->buffers[i].size) + "\n"
            "buffer data: " + (data->buffers[i].data ? "VALID" : "NULL"));
    }
    */

    vector<ImportNodeData> nodeData{};

    for (cgltf_size nodeIndex = 0; nodeIndex < data->nodes_count; ++nodeIndex)
    {
        cgltf_node& node = data->nodes[nodeIndex];

        if (!node.mesh)
        {
            Log::Print(
                "Skipped importing node '" + to_string(nodeIndex) + "' because it had no geometry!",
                "KG_IMPORT_MESH",
                LogType::LOG_WARNING);

            continue;
        }

        cgltf_mesh& meshContainer = *node.mesh;
        string nodeName = meshContainer.name ? meshContainer.name : "unnamed_node";

        if (meshContainer.primitives_count == 0)
        {
            Log::Print(
                "Skipped importing node '" + to_string(nodeIndex) 
                + "' with name '" + nodeName + "' because it had no primitives!",
                "KG_IMPORT_MESH",
                LogType::LOG_WARNING);

            continue;
        }

        vector<pair<cgltf_primitive*, ImportPrimitiveData>> primitives{};

        for (cgltf_size primitiveIndex = 0; primitiveIndex < meshContainer.primitives_count; ++primitiveIndex)
        {
            ImportMeshData meshData{};

            cgltf_primitive& primitive = meshContainer.primitives[primitiveIndex];

            if (primitive.type != cgltf_primitive_type_triangles)
            {
                Log::Print(
                    "Skipped importing primitive '" + to_string(primitiveIndex) 
                    + "' in node '" + to_string(nodeIndex) 
                    + "' with name '" + nodeName + "' because it has unsupported primitive topology!",
                    "KG_IMPORT_MESH",
                    LogType::LOG_WARNING);

                continue;
            }

            //
            // GET ACCESSORS
            //

            cgltf_accessor* posAccessor{};
            cgltf_accessor* normalAccessor{};
            cgltf_accessor* uvAccessor{};
            cgltf_accessor* colorAccessor{};

            for (cgltf_size attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex)
            {
                cgltf_attribute& attribute = primitive.attributes[attributeIndex];

                switch (attribute.type)
                {
                    default: break;

                    case cgltf_attribute_type_position:
                        posAccessor = attribute.data;
                        break;
                    case cgltf_attribute_type_normal:
                        normalAccessor = attribute.data;
                        break;
                    case cgltf_attribute_type_texcoord:
                        if (attribute.index == 0) uvAccessor = attribute.data;
                        break;
                    case cgltf_attribute_type_color:
                        if (attribute.index == 0) colorAccessor = attribute.data;
                        break;
                }
            }

            if (!posAccessor)
            {
                Log::Print(
                    "Skipped importing primitive '" + to_string(primitiveIndex) 
                    + "' under node '" + to_string(nodeIndex) 
                    + "' with name '" + nodeName + "' because the primitive has no vertex positions!",
                    "KG_IMPORT_MESH",
                    LogType::LOG_WARNING);

                continue;
            }

            if (colorAccessor
                && colorAccessor->type != cgltf_type_vec3
                && colorAccessor->type != cgltf_type_vec4)
            {
                Log::Print(
                    "Skipped importing primitive '" + to_string(primitiveIndex) 
                    + "' under node '" + to_string(nodeIndex) 
                    + "' with name '" + nodeName + "' because the mesh color type is unsupported!",
                    "KG_IMPORT_MESH",
                    LogType::LOG_WARNING);

                continue;
            }

            //
            // GET VERTICES
            //

            meshData.vertices.resize(posAccessor->count);

            //positions
            for (cgltf_size vertexIndex = 0; vertexIndex < posAccessor->count; ++vertexIndex)
            {
                Vertex& vertex = meshData.vertices[vertexIndex];

                f32 pos[3]{};

                cgltf_accessor_read_float(
                    posAccessor,
                    vertexIndex,
                    pos,
                    3);

                vertex.pos = { pos[0], pos[1], pos[2] };

                //normals
                if (normalAccessor)
                {
                    f32 norm[3]{};

                    cgltf_accessor_read_float(
                        normalAccessor,
                        vertexIndex,
                        norm,
                        3);

                    vertex.normal = { norm[0], norm[1], norm[2] };
                }

                //uvs
                if (uvAccessor)
                {
                    f32 uv[2]{};

                    cgltf_accessor_read_float(
                        uvAccessor,
                        vertexIndex,
                        uv,
                        2);

                    vertex.uv = { uv[0], uv[1] };
                }

                //colors
                if (colorAccessor)
                {
                    f32 color[4]{ 1.0f, 1.0f, 1.0f, 1.0f };

                    cgltf_accessor_read_float(
                        colorAccessor,
                        vertexIndex,
                        color,
                        colorAccessor->type == cgltf_type_vec3 ? 3 : 4);

                    vertex.color = { color[0], color[1], color[2], color[3] };
                }
            }

            /*
            //print first 5 vertex positions
            for (size_t i = 0; i < min<size_t>(meshData.vertices.size(), 5); ++i)
            {
                const Vertex& v = meshData.vertices[i];
            
                Log::Print(
                    "@@@@@\n"
                    "vertex " + to_string(i) + ": "
                    + to_string(v.pos.x) + ", "
                    + to_string(v.pos.y) + ", "
                    + to_string(v.pos.z));
            }
            */

            //
            // GET INDICES
            //

            if (primitive.indices)
            {
                meshData.indices.reserve(primitive.indices->count);

                for (cgltf_size i = 0; i < primitive.indices->count; ++i)
                {
                    meshData.indices.push_back(scast<u32>(cgltf_accessor_read_index(
                        primitive.indices,
                        i)));
                }
            }

            /*
            Log::Print(
                "@@@@@\n"
                "node name: " + nodeName + "\n"
                "node index: " + to_string(nodeIndex) + "\n"
                "primitive index: " + to_string(primitiveIndex) + "\n"
                "primitive vertices size: " + to_string(meshData.vertices.size()) + "\n"
                "primitive indices size: " + to_string(meshData.indices.size()));
            */

            primitives.push_back( { &primitive,  { .meshData = std::move(meshData) }} );
        }

        if (primitives.empty())
        {
            Log::Print(
                "Skipped importing node '" + to_string(nodeIndex) 
                + "' with name '" + nodeName + "' because it had no valid primitives!",
                "KG_IMPORT_MESH",
                LogType::LOG_WARNING);

            continue;
        }

        //
        // SCAN MATERIALS
        //

        for (auto& [mesh, import] : primitives)
        {
            ImportMaterialData matData{};

            if (mesh->material)
            {
                cgltf_material& material = *mesh->material;

                matData.materialName =
                    material.name
                    ? material.name
                    : "unnamed_material";

                if (material.has_pbr_metallic_roughness)
                {
                    const cgltf_float* baseColor = material.pbr_metallic_roughness.base_color_factor;

                    matData.baseColor = 
                    {
                        baseColor[0],
                        baseColor[1],
                        baseColor[2],
                        baseColor[3]
                    };

                    cgltf_texture_view& textureView = material.pbr_metallic_roughness.base_color_texture;

                    if (textureView.texture)
                    {
                        if (!GetPixelData(
                            nodeIndex,
                            nodeName,
                            matData.materialName,
                            *textureView.texture,
                            matData.textureData))
                        {
                            continue;
                        }
                    }
                }

                switch (material.alpha_mode)
                {
                    default:
                    case cgltf_alpha_mode_opaque:
                        matData.alphaMode = AlphaMode::A_OPAQUE;
                        break;
                    case cgltf_alpha_mode_blend:
                        matData.alphaMode = AlphaMode::A_BLEND;
                        break;
                    case cgltf_alpha_mode_mask:
                        matData.alphaMode = AlphaMode::A_MASK;
                        break;
                }

                matData.alphaCutoff = material.alpha_cutoff;

                /*
                Log::Print(
                    "@@@@@\n"
                    "node: " + nodeName + "\n"
                    "material: " + matData.materialName + "\n"
                    "base color: "
                    + to_string(matData.baseColor.x) + ", "
                    + to_string(matData.baseColor.y) + ", "
                    + to_string(matData.baseColor.z) + ", "
                    + to_string(matData.baseColor.w));
                */
            }

            import.matData = std::move(matData);
        }

        ImportNodeData newNodeData{ .nodeName = nodeName };

        cgltf_float worldMatrix[16]{};

        cgltf_node_transform_world(&node, worldMatrix);

        mat4 worldMat{ worldMatrix };

        newNodeData.transform.setpos(topos(worldMat));
        //safely converts glb XYZW to math_utils quat WXYZ
        newNodeData.transform.setrotquat(toquat(worldMat));
        newNodeData.transform.setsize(tosize(worldMat));

        /*
        auto bool_to_string = [](bool value) -> string
            {
                return value ? "true" : "false";
            };

        Log::Print(
            "@@@@@\n"
            "NODE TRANSFORM\n"
            "node: " + to_string(nodeIndex) + "\n"
            "has_matrix: " + bool_to_string(node.has_matrix) + "\n"
            "has_translation: " + bool_to_string(node.has_translation) + "\n"
            "translation: "
            + to_string(node.translation[0]) + ", "
            + to_string(node.translation[1]) + ", "
            + to_string(node.translation[2]) + "\n"
            "WORLD MATRIX:\n"
            + to_string(worldMatrix[0]) + ", "
            + to_string(worldMatrix[1]) + ", "
            + to_string(worldMatrix[2]) + ", "
            + to_string(worldMatrix[3]) + "\n"
            + to_string(worldMatrix[4]) + ", "
            + to_string(worldMatrix[5]) + ", "
            + to_string(worldMatrix[6]) + ", "
            + to_string(worldMatrix[7]) + "\n"
            + to_string(worldMatrix[8]) + ", "
            + to_string(worldMatrix[9]) + ", "
            + to_string(worldMatrix[10]) + ", "
            + to_string(worldMatrix[11]) + "\n"
            + to_string(worldMatrix[12]) + ", "
            + to_string(worldMatrix[13]) + ", "
            + to_string(worldMatrix[14]) + ", "
            + to_string(worldMatrix[15]));
        */

        newNodeData.primitiveData.reserve(primitives.size());
        for (auto& [_, pdata] : primitives)
        {
            newNodeData.primitiveData.push_back(std::move(pdata));
        }

        nodeData.push_back(std::move(newNodeData));
    }

    //
    // FINISH
    //

    if (nodeData.empty()) return "Failed to import GLTF/GLB because it had no valid mesh data!";

    outNodeData = std::move(nodeData);

    cgltf_free(data);

    return "";
}

bool GetPixelData(
    const u32 nodeIndex,
    const string& nodeName,
    const string& materialName,
    cgltf_texture& texture,
    ImportTextureData& texData)
{
    auto set_fallback_texture = [&texData]() -> u32
        {
            Shader* first = Shader::GetRegistry().GetAllContent().front();

            Texture* fallback{};
            string err = Texture::GetRegistry().GetContent(first->GetFallbackTextureID(), fallback);
            if (!err.empty())
            {
                KalaGraphicsCore::ForceClose(
                    "Import mesh error",
                    "Failed to import mesh because shader '" + to_string(first->GetID()) 
                    + "' fallback texture was invalid! Reason: " + err);
            }

            texData = 
            {
                .pixelData = fallback->GetPixelData(),
                .size = fallback->GetSize(),
                .pixelFormat = fallback->GetPixelFormat()
            };

            return fallback->GetID();
        };

    auto verify_image_uri = [
        &nodeIndex,
        &nodeName,
        &materialName,
        &texData,
        set_fallback_texture](cgltf_image& image) -> void
        {
            string ext = path(image.uri).extension().string();

            if (ext != ".png")
            {
                u32 fallbackID = set_fallback_texture();

                Log::Print(
                    "Node '" + to_string(nodeIndex) 
                    + "' with name '" + nodeName + "' material '" + materialName 
                    + "' texture '" + path(image.uri).string() + "' extension '" + ext + "' is unsupported! "
                    "Assigning fallback texture '" + to_string(fallbackID) + "'.",
                    "KG_MESH",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            ImportTexture* importTexture = ImportTexture::Initialize(path(image.uri));

            if (importTexture)
            {
                texData = importTexture->GetTextureData();
                importTexture->Destroy();

                return;
            }

            u32 fallbackID = set_fallback_texture();

            Log::Print(
                "Node '" + to_string(nodeIndex) 
                    + "' with name '" + nodeName + "' material '" + materialName 
                + "' texture '" + path(image.uri).string() + "' was invalid! "
                "Assigning fallback texture '" + to_string(fallbackID) + "'.",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);
        };

    auto verify_image_buffer_view = [
        &nodeIndex,
        &nodeName,
        &materialName,
        &texData,
        set_fallback_texture](
        cgltf_image& image,
        bool& outResult) -> void
        {
            if (!image.mime_type)
            {
                u32 fallbackID = set_fallback_texture();

                Log::Print(
                    "Node '" + to_string(nodeIndex) 
                    + "' with name '" + nodeName + "' material '" + materialName 
                    + "' embedded texture has invalid mime type! "
                    "Assigning fallback texture '" + to_string(fallbackID) + "'.",
                    "KG_MESH",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            if (string(image.mime_type) != "image/png")
            {
                u32 fallbackID = set_fallback_texture();

                Log::Print(
                    "Node '" + to_string(nodeIndex) 
                    + "' with name '" + nodeName + "' material '" + materialName 
                    + "' embedded texture mime type '" + image.mime_type + "' is not supported! "
                    "Assigning fallback texture '" + to_string(fallbackID) + "'.",
                    "KG_MESH",
                    LogType::LOG_ERROR,
                    2);

                return;
            }

            cgltf_buffer_view& bufferView = *image.buffer_view;

            if (!bufferView.buffer
                || !bufferView.buffer->data)
            {
                Log::Print(
                    "Skipped importing node '" + to_string(nodeIndex) 
                    + "' with name '" + nodeName + "' because its buffer was invalid!",
                    "KG_IMPORT_MESH",
                    LogType::LOG_WARNING);

                outResult = false;
                return;
            }

            const u8* imageStart = 
                scast<const u8*>(bufferView.buffer->data)
                + bufferView.offset;

            vector<u8> imageData(
                imageStart,
                imageStart + bufferView.size);

            ImportTexture* importTexture = ImportTexture::Initialize(std::move(imageData));

            if (importTexture)
            {
                texData = importTexture->GetTextureData();
                importTexture->Destroy();

                return;
            }

            u32 fallbackID = set_fallback_texture();

            Log::Print(
                "Node '" + to_string(nodeIndex) 
                + "' with name '" + nodeName + "' material '" + materialName 
                + "' embedded texture was invalid! "
                "Assigning fallback texture '" + to_string(fallbackID) + "'.",
                "KG_MESH",
                LogType::LOG_ERROR,
                2);
        };

    if (!texture.image)
    {
        u32 fallbackID = set_fallback_texture();

        Log::Print(
            "Node '" + to_string(nodeIndex) 
            + "' with name '" + nodeName + "' material '" + materialName 
            + "' texture was invalid! "
            "Assigning fallback texture '" + to_string(fallbackID) + "'.",
            "KG_MESH",
            LogType::LOG_ERROR,
            2);

        return true;
    }

    cgltf_image& image = *texture.image;

    if (image.uri)
    {
        verify_image_uri(image);
    }
    else if (image.buffer_view)
    {
        bool result = true;
        verify_image_buffer_view(image, result);

        if (!result) return false;
    }
    else
    {
        u32 fallbackID = set_fallback_texture();

        Log::Print(
            "Node '" + to_string(nodeIndex) 
            + "' with name '" + nodeName + "' material '" + materialName 
            + "' texture had no URI or buffer view! "
            "Assigning fallback texture '" + to_string(fallbackID) + "'.",
            "KG_MESH",
            LogType::LOG_ERROR,
            2);

        return true;
    }

    return true;
}