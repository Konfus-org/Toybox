#include "ExampleLayer.h"
#include <Tbx/Core/Rendering/RenderingAPI.h>
#include <Tbx/Runtime/Input/Input.h>
#include <Tbx/Runtime/Input/InputCodes.h>
#include <Tbx/Runtime/Time/DeltaTime.h>
#include <Tbx/Core/Math/Transform.h>
#include <Tbx/Core/TBS/Toy.h>
#include <Tbx/Core/TBS/World.h>

std::shared_ptr<Tbx::PlaySpace> _playSpace = nullptr;
Tbx::Toy _mainCam = {};

void ExampleLayer::OnAttach()
{
	TBX_TRACE("Test scene attached!");

	// Setup testing scene...
    auto playSpaceId = Tbx::World::MakePlayspace();
	_playSpace = Tbx::World::GetPlayspace(playSpaceId);

    // Create checkboard toy
	Tbx::Toy checkerBox2dToy = _playSpace->MakeToy("Checkerboard");
	_playSpace->AddBlockTo<Tbx::Mesh>(checkerBox2dToy);
	_playSpace->AddBlockTo<Tbx::Transform>(checkerBox2dToy);
	auto& material = _playSpace->AddBlockTo<Tbx::Material>(checkerBox2dToy);
    material.SetTexture(0, Tbx::Texture("Assets/Checkerboard.png"));
	material.SetColor(Tbx::Colors::Red);

	// Create camera toy
	_mainCam = _playSpace->MakeToy("Camera");
	_playSpace->AddBlockTo<Tbx::Camera>(_mainCam);
	auto& transform = _playSpace->AddBlockTo<Tbx::Transform>(_mainCam);
	transform.Position = { 0, 0, -10 };

	// Opens our new playSpace
	_playSpace->Open();
}

void ExampleLayer::OnDetach()
{
	TBX_TRACE("Test scene detached!");
}

void ExampleLayer::OnUpdate()
{
	const auto& deltaTime = Tbx::Time::DeltaTime::Seconds();

	// Camera movement
	auto& camTransform = _playSpace->GetBlockOn<Tbx::Transform>(_mainCam);

	auto cameraSpeedMod = 1.0f;
	if (Tbx::Input::IsKeyDown(TBX_KEY_LEFT_SHIFT))
	{
		cameraSpeedMod = 4.0f;
	}

	if (Tbx::Input::IsKeyDown(TBX_KEY_W))
	{
		camTransform.Position = camTransform.Position + 
			Tbx::Quaternion::GetForward(camTransform.Rotation) * (_camMoveSpeed * cameraSpeedMod * deltaTime);
	}
    else if (Tbx::Input::IsKeyDown(TBX_KEY_S))
    {
		camTransform.Position = camTransform.Position + 
			(Tbx::Quaternion::GetForward(camTransform.Rotation) * -1) * (_camMoveSpeed * cameraSpeedMod * deltaTime);
    }

    if (Tbx::Input::IsKeyDown(TBX_KEY_A))
    {
		camTransform.Position = camTransform.Position + 
			(Tbx::Quaternion::GetRight(camTransform.Rotation) * -1) * (_camMoveSpeed * cameraSpeedMod * deltaTime);
    }
    else if (Tbx::Input::IsKeyDown(TBX_KEY_D))
    {
		camTransform.Position = camTransform.Position + 
			Tbx::Quaternion::GetRight(camTransform.Rotation) * (_camMoveSpeed * cameraSpeedMod * deltaTime);
    }

    if (Tbx::Input::IsKeyDown(TBX_KEY_Q))
    {
		camTransform.Position = camTransform.Position + 
			Tbx::WorldSpace::Down * (_camMoveSpeed * cameraSpeedMod * deltaTime);
    }
    else if (Tbx::Input::IsKeyDown(TBX_KEY_E))
    {
		camTransform.Position = camTransform.Position + 
			Tbx::WorldSpace::Up * (_camMoveSpeed * cameraSpeedMod * deltaTime);
    }

	// Camera rotation
	if (Tbx::Input::IsKeyDown(TBX_KEY_LEFT))
	{
		auto rotationToApply = Tbx::Quaternion::FromAxisAngle(Tbx::WorldSpace::Up, -1.0f * (_camRotateSpeed * deltaTime));
		camTransform.Rotation = Tbx::Quaternion::Normalize(rotationToApply * camTransform.Rotation);
	}
	else if (Tbx::Input::IsKeyDown(TBX_KEY_RIGHT))
	{
		auto rotationToApply = Tbx::Quaternion::FromAxisAngle(Tbx::WorldSpace::Up, 1.0f * (_camRotateSpeed * deltaTime));
		camTransform.Rotation = Tbx::Quaternion::Normalize(rotationToApply * camTransform.Rotation);
	}
	else if (Tbx::Input::IsKeyDown(TBX_KEY_UP))
	{
		auto rotationToApply = Tbx::Quaternion::FromAxisAngle(Tbx::Quaternion::GetRight(camTransform.Rotation), 1.0f * (_camRotateSpeed * deltaTime));
		camTransform.Rotation = Tbx::Quaternion::Normalize(rotationToApply * camTransform.Rotation);
	}
	else if (Tbx::Input::IsKeyDown(TBX_KEY_DOWN))
	{
		auto rotationToApply = Tbx::Quaternion::FromAxisAngle(Tbx::Quaternion::GetRight(camTransform.Rotation), -1.0f * (_camRotateSpeed * deltaTime));
		camTransform.Rotation = Tbx::Quaternion::Normalize(rotationToApply * camTransform.Rotation);
	}

	TBX_INFO("Cam Right: {}", Tbx::Quaternion::GetRight(camTransform.Rotation).ToString());

	auto eulerAngles = Tbx::Quaternion::ToEuler(camTransform.Rotation);
	float pitch = eulerAngles.X;
	if (pitch > 89.0f)
		camTransform.Rotation = Tbx::Quaternion(Tbx::Vector3(89.0f, eulerAngles.Y, eulerAngles.Z));
	if (pitch < -89.0f)
		camTransform.Rotation = Tbx::Quaternion(Tbx::Vector3(-89.0f, eulerAngles.Y, eulerAngles.Z));

}
