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
using KalaGraphics::Import::ImportShaderData;

using std::string;
using std::string_view;
using std::to_string;
using std::vector;
using std::unique_ptr;
using std::make_unique;
using std::filesystem::exists;
using std::filesystem::is_regular_file;

static constexpr string_view EXT_VERT = ".vert";
static constexpr string_view EXT_FRAG = ".frag";
static constexpr string_view EXT_GEOM = ".geom";
static constexpr string_view EXT_SPV = ".spv";

static string Init_SPV(
    vector<u8>&& binaryData,
    ImportShaderData& outShaderData);

namespace KalaGraphics::Import
{
    static KalaGraphicsRegistry<ImportShader> registry{};

    KalaGraphicsRegistry<ImportShader>& ImportShader::GetRegistry() { return registry; }

    void ImportShader::Compile(
        path&& inPath,
        path&& outPath,
        bool overwrite)
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
        if (!overwrite
            && exists(outPath))
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
            && ext != EXT_FRAG
            && ext != EXT_GEOM)
        {
            Log::Print(
                "Failed to compile shader '" + inPath.string() + "' because its extension is not supported!",
                "KG_IMPORT_SHADER",
                LogType::LOG_ERROR,
                2);

            return;
        }

#if defined(KWIN_ANY)
        int glslcResult = system("glslc --version > NUL 2>&1");
#else
        int glslcResult = system("glslc --version > /dev/null 2>&1");
#endif

        if (glslcResult != 0)
        {
            Log::Print(
                "Failed to compile shader '" + inPath.string() + "' because glslc was not found or could not be executed!",
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

#if defined(KWIN_ANY)
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
        if (ext != EXT_SPV)
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

        ImportShaderData shaderData{};
        if (ext == EXT_SPV)
        {
            errMsg = Init_SPV(
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

        string err = registry.AddContent(newID, std::move(newShader));
        if (!err.empty())
        {
			KalaGraphicsCore::ForceClose(
				"KalaGraphics import shader error",
				"Failed to initialize import shader! Reason: " + err);
        }

        Log::Print(
			"Created new import shader '" + to_string(newID) + "'!",
			"KG_IMPORT_SHADER",
			LogType::LOG_SUCCESS);

        return shaderPtr;
    }

    u32 ImportShader::GetID() const { return ID; }

    void ImportShader::Destroy()
    {
        string err = registry.DestroyContent(ID);
        if (!err.empty())
        {
            KalaGraphicsCore::ForceClose(
                "KalaGraphics import shader error",
                "Failed to destroy import shader '" + to_string(ID) + "'! Reason: " + err);
        }
    }

    ImportShader::~ImportShader()
    {
        Log::Print(
            "Destroying import shader data '" + to_string(ID) + "'.",
            "KG_IMPORT_SHADER",
            LogType::LOG_INFO);
    }
}

string Init_SPV(
    vector<u8>&& binaryData,
    ImportShaderData& outShaderData)
{
    return "";
}