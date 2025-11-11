//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <memory>

#include "KalaHeaders/log_utils.hpp"

#include "core/core.hpp"
#include "graphics/camera.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;

using KalaGraphics::Core::KalaGraphicsCore;

using std::to_string;
using std::unique_ptr;
using std::make_unique;

namespace KalaGraphics::Graphics
{
	Camera* Camera::Initialize(
		const string& cameraName,
		vec2 framebufferSize,
		f32 fov,
		f32 speed,
		const vec3& pos,
		const vec3& rot)
	{
		OpenGL_Context* context{};

		if (!context
			|| !context->IsInitialized())
		{
			Log::Print(
				"Cannot load camera because its OpenGL context is invalid!",
				"CAMERA",
				LogType::LOG_ERROR);

			return nullptr;
		}

		u32 newID = ++KalaGraphicsCore::globalID;
		unique_ptr<Camera> newCam = make_unique<Camera>();
		Camera* camPtr = newCam.get();

		Log::Print(
			"Creating camera '" + cameraName + "' with ID '" + to_string(newID) + "'.",
			"CAMERA",
			LogType::LOG_DEBUG);

		camPtr->SetFOV(fov);
		camPtr->SetSpeed(speed);

		camPtr->SetPos(pos);
		camPtr->SetRotVec(rot);
		camPtr->UpdateCameraRotation(vec2(0));

		f32 aspectRatio = framebufferSize.x / framebufferSize.y;
		camPtr->SetAspectRatio(aspectRatio);

		registry.AddContent(newID, move(newCam));

		camPtr->name = cameraName;
		camPtr->ID = newID;

		camPtr->isInitialized = true;

		Log::Print(
			"Created camera '" + cameraName + "' with ID '" + to_string(newID) + "'!",
			"CAMERA",
			LogType::LOG_SUCCESS);

		return camPtr;
	}

	Camera::~Camera()
	{
		if (!isInitialized)
		{
			Log::Print(
				"Cannot destroy camera '" + name + "' with ID '" + to_string(ID) + "' because it is not initialized!",
				"CAMERA",
				LogType::LOG_ERROR,
				2);

			return;
		}

		Log::Print(
			"Destroying camera '" + name + "' with ID '" + to_string(ID) + "'.",
			"CAMERA",
			LogType::LOG_INFO);
	}
}