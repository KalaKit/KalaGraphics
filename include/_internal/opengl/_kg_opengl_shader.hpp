//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <array>
#include <filesystem>

#include "core_utils.hpp"
#include "math_utils.hpp"

#include "core/kg_registry.hpp"

namespace KalaGraphics::Internal::OpenGL
{
	using std::string;
	using std::string_view;
	using std::array;
	using std::filesystem::path;

	using KalaHeaders::KalaMath::vec2;
	using KalaHeaders::KalaMath::vec3;
	using KalaHeaders::KalaMath::vec4;
	using KalaHeaders::KalaMath::mat2;
	using KalaHeaders::KalaMath::mat3;
	using KalaHeaders::KalaMath::mat4;
	
	using KalaGraphics::Core::KalaGraphicsRegistry;

	enum class OpenGL_ShaderType
	{
		SHADER_NONE,
		SHADER_VERTEX,
		SHADER_FRAGMENT,
		SHADER_GEOMETRY
	};

	struct LIB_API OpenGL_ShaderData
	{
		//either fill shader data or shader path,
		//it picks data by default if both are filled
		path shaderPath{};

		//either fill shader data or shader path,
		//it picks data by default if both are filled
		string shaderData{};

		//assign to something else than the default 'SHADER_NONE'
		OpenGL_ShaderType type = OpenGL_ShaderType::SHADER_NONE;

		//ID of the shader, does not need to be filled by user
		u32 ID{};
	};

	class LIB_API OpenGL_Shader
	{
	public:
		static KalaGraphicsRegistry<OpenGL_Shader>& GetRegistry();

		//Toggle verbose logging. If true, then usually frequently updated runtime values like
		//vertex, fragment and geometry shader compilation messages will dump their logs into the console.
		static void SetVerboseLoggingState(bool newState);
		static bool IsVerboseLoggingEnabled();

		//Create a new shader with up to three types of shader files.
		//Geometry shaders are optional but vert and frag shader must always be filled
		static OpenGL_Shader* Initialize(
			uintptr_t glContext,
			string_view shaderName,
			const array<OpenGL_ShaderData, 3>& shaderData);

		bool IsInitialized() const;

		const string& GetName() const;
		bool SetName(string_view newName);

		//Returns the global ID of this shader
		u32 GetID() const;
		//Returns the context ID of the context this shader uses
		u32 GetContextID() const;

		//Returns the OpenGL program ID of this shader
		u32 GetProgramID() const;

		//Returns shader data assigned to shader type
		const string& GetShaderData(OpenGL_ShaderType targetType) const;
		//Returns shader path assigned to shader type
		const path& GetShaderPath(OpenGL_ShaderType targetType) const;

		u32 GetShaderID(OpenGL_ShaderType targetType) const;

		//Bind current shader
		bool Bind();

		bool HotReload();

		void SetBool(string_view name, bool value) const;
		void SetInt(string_view name, i32 value) const;
		void SetFloat(string_view name, f32 value) const;

		void SetVec2(string_view name, const vec2& value) const;
		void SetVec3(string_view name, const vec3& value) const;
		void SetVec4(string_view name, const vec4& value) const;

		void SetMat2(string_view name, const mat2& mat) const;
		void SetMat3(string_view name, const mat3& mat) const;
		void SetMat4(string_view name, const mat4& mat) const;

		//Do not destroy manually, erase from registry instead
		~OpenGL_Shader();
	private:
		bool isInitialized{};

		string name{};

		u32 ID{};
		u32 contextID{};

		u32 programID{};

		OpenGL_ShaderData vertData{};
		OpenGL_ShaderData fragData{};
		OpenGL_ShaderData geomData{};
	};
}