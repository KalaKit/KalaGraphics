//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <filesystem>

#include "core_utils.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::Import
{
    using KalaGraphics::Core::KalaGraphicsRegistry;

    using std::filesystem::path;
    using std::vector;
    using std::string;
    using std::string_view;
    using std::default_delete;

    static constexpr string_view EXT_VERT = ".vert";
    static constexpr string_view EXT_FRAG = ".frag";

    static constexpr string_view EXT_SPV = ".spv";
    static constexpr string_view EXT_KSHA = ".ksha";

    struct ShaderData
    {

    };

    class LIB_API ImportShader
    {
    friend struct default_delete<ImportShader>;
    public:
        KNODISCARD
		static KalaGraphicsRegistry<ImportShader>& GetRegistry();

        //Compile a raw GLSL 4.6 Vulkan shader into spirv with glslc
        static void Compile(
            path&& inPath,
            path&& outPath);

        KNODISCARD
		static ImportShader* Initialize(path&& shaderPath);

        KNODISCARD
		u32 GetID() const;

        void Destroy();
    private:
        ~ImportShader();

        KNODISCARD
		static string Init_SPV(
            vector<u8>&& binaryData,
            ShaderData& outShaderData);
        KNODISCARD
		static string Init_KSHA(
            vector<u8>&& binaryData,
            ShaderData& outShaderData);

        u32 ID{};

        path shaderPath{};
        ShaderData shaderData{};
    };
}