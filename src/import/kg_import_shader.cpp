//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "log_utils.hpp"
#include "file_utils.hpp"

#include "import/kg_import_shader.hpp"
#include "core/kg_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaFile::ReadBinaryDataFromFile;

using KalaGraphics::Core::KalaGraphicsCore;

using std::string;
using std::to_string;
using std::unique_ptr;
using std::make_unique;
using std::filesystem::exists;
using std::filesystem::is_regular_file;

namespace KalaGraphics::Import
{
    static KalaGraphicsRegistry<ImportShader> registry{};

    KalaGraphicsRegistry<ImportShader>& ImportShader::GetRegistry() { return registry; }

    void ImportShader::Compile(
        path&& inPath,
        path&& outPath)
    {
        if (inPath.empty())
        {
            Log::Print(
                "Failed to compile shader because in path was empty!",
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (outPath.empty())
        {
            Log::Print(
                "Failed to compile shader because out path was empty!",
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (inPath == outPath)
        {
            Log::Print(
                "Failed to compile shader '" + inPath.string() + "' because it was the same as out path!",
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (!exists(inPath))
        {
            Log::Print(
                "Failed to compile shader '" + inPath.string() + "' because it does not exist!",
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (exists(outPath))
        {
            Log::Print(
                "Failed to compile shader '" + inPath.string() + "' because out path '" + outPath.string() + "' already exists!",
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }

        if (!is_regular_file(inPath))
        {
            Log::Print(
                "Failed to compile shader '" + inPath.string() + "' because it is not a regular file!",
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }

        string ext = inPath.extension().string();
        if (ext != EXT_VERT
            && ext != EXT_FRAG)
        {
            Log::Print(
                "Failed to compile shader '" + inPath.string() + "' because its extension is not supported!",
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }

        string command = "glslc --target-env=vulkan1.4 \"" 
            + inPath.string() + "\" -o \"" 
            + outPath.string() + "\" 2>&1";

        string errMsg{};
        char buffer[256]{};

#ifdef _WIN32
        FILE* pipe = _popen(command.c_str(), "r");
        if (!pipe)
        {
            Log::Print(
                "Failed to compile shader '" + inPath.string() + "' because _popen failed!",
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }

        while(fgets(buffer, sizeof(buffer), pipe)) errMsg += buffer;

        int exitCode = _pclose(pipe);
#else
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe)
        {
            Log::Print(
                "Failed to compile shader '" + inPath.string() + "' because popen failed!",
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }

        while(fgets(buffer, sizeof(buffer), pipe)) errMsg += buffer;

        int exitCode = WEXITSTATUS(pclose(pipe));
#endif

        if (exitCode != 0)
        {
            Log::Print(
                "Failed to compile shader '" + inPath.string() 
                + "' to target '" + outPath.string() 
                + "'! Reason: [" + to_string(exitCode) + "] " + errMsg,
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }

        Log::Print(
            "Compiled shader '" + inPath.string() + "' to output '" + outPath.string() + "'!",
            "KG_IMPORT_SHADER",
            LogType::LOG_SUCCESS);
    }

    ImportShader* ImportShader::Initialize(path&& shaderPath)
    {
        if (!is_regular_file(shaderPath))
        {
            Log::Print(
                "Failed to import shader '" + shaderPath.string() + "' because it is not a regular file!",
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        string ext = shaderPath.extension().string();
        if (ext != EXT_SPV
            && ext != EXT_KSHA)
        {
            Log::Print(
                "Failed to import shader '" + shaderPath.string() + "' because its extension is not supported!",
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        vector<u8> outData{};
        string errMsg = ReadBinaryDataFromFile(
            shaderPath,
            outData);

        if (!errMsg.empty())
        {
            Log::Print(
                "Failed to import shader '" + shaderPath.string() + "'! Reason: " + errMsg,
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        ShaderData shaderData{};
        if (ext == ".png")
        {
            errMsg = Init_SPV(
                std::move(outData),
                shaderData);
        }
        else
        {
            errMsg = Init_KSHA(
                std::move(outData),
                shaderData);
        }

        if (!errMsg.empty())
        {
            Log::Print(
                "Failed to import shader '" + shaderPath.string() + "'! Reason: " + errMsg,
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return nullptr;
        }

        unique_ptr<ImportShader> newShader = make_unique<ImportShader>();
        ImportShader* shaderPtr = newShader.get();

        u32 newID = KalaGraphicsCore::GetGlobalID() + 1;
        KalaGraphicsCore::SetGlobalID(newID);

        shaderPtr->ID = newID;
        shaderPtr->shaderPath = std::move(shaderPath);
        shaderPtr->shaderData = std::move(shaderData);

        registry.AddContent(newID, std::move(newShader));

        Log::Print(
			"Created new import shader '" + to_string(newID) + "'!",
			"KG_IMPORT_SHADER",
			LogType::LOG_SUCCESS);

        return shaderPtr;
    }

    u32 ImportShader::GetID() const { return ID; }

    string ImportShader::Init_SPV(
        vector<u8>&& binaryData,
        ShaderData& outShaderData)
    {
        return "init spv";
    }
    string ImportShader::Init_KSHA(
        vector<u8>&& binaryData,
        ShaderData& outShaderData)
    {
        return "init ksha";
    }

    void ImportShader::Destroy()
    {
        registry.RemoveContent(ID);
    }

    ImportShader::~ImportShader()
    {
        Log::Print(
            "Destroying import shader data '" + to_string(ID) + "'.",
            "KG_IMPORT_SHADER",
            LogType::LOG_INFO);
    }
}