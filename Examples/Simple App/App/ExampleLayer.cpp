#include "ExampleLayer.h"
#include <Tbx/Core/Rendering/RenderingAPI.h>
#include <Tbx/Runtime/Input/Input.h>
#include <Tbx/Runtime/Input/InputCodes.h>
#include <Tbx/Runtime/Time/DeltaTime.h>
#include <Tbx/Core/Math/Transform.h>
#include <Tbx/Core/TBS/Toy.h>
#include <Tbx/Core/TBS/World.h>

std::shared_ptr<Tbx::PlaySpace> _playspace = nullptr;
Tbx::Toy _mainCam = {};

void ExampleLayer::OnAttach()
{
	TBX_TRACE("Test scene attached!");

	// Setup testing scene...
    auto playSpaceId = Tbx::World::MakePlayspace();
	_playspace = Tbx::World::GetPlayspace(playSpaceId);

    // Create checkboard toy
	Tbx::Toy checkerBox2dToy = _playspace->MakeToy("Checkerboard");
	_playspace->AddBlockTo<Tbx::Mesh>(checkerBox2dToy);
	_playspace->AddBlockTo<Tbx::Transform>(checkerBox2dToy);
	auto& material = _playspace->AddBlockTo<Tbx::Material>(checkerBox2dToy);
    material.SetTexture(0, Tbx::Texture("Assets/Checkerboard.png"));
	material.SetColor(Tbx::Colors::Red);

	// Create camera toy
	_mainCam = _playspace->MakeToy("Camera");
	_playspace->AddBlockTo<Tbx::Camera>(_mainCam);
	auto& transform = _playspace->AddBlockTo<Tbx::Transform>(_mainCam);
	transform.Position = { 0, 0, -10 };

	// Opens our new playspace
	_playspace->Open();
}

void ExampleLayer::OnDetach()
{
	TBX_TRACE("Test scene detached!");
}

void ExampleLayer::OnUpdate()
{
	const auto& deltaTime = Tbx::Time::DeltaTime::Seconds();

	// Camera movement
	auto& camTransform = _playspace->GetBlockOn<Tbx::Transform>(_mainCam);
	if (Tbx::Input::IsKeyDown(TBX_KEY_W))
	{
		camTransform.Position = camTransform.Position + Tbx::Vector3::Forward() * _camMoveSpeed * deltaTime;
	}
    else if (Tbx::Input::IsKeyDown(TBX_KEY_S))
    {
		camTransform.Position = camTransform.Position + Tbx::Vector3::Backward() * _camMoveSpeed * deltaTime;
    }

    if (Tbx::Input::IsKeyDown(TBX_KEY_A))
    {
		camTransform.Position = camTransform.Position + Tbx::Vector3::Left() * _camMoveSpeed * deltaTime;
    }
    else if (Tbx::Input::IsKeyDown(TBX_KEY_D))
    {
		camTransform.Position = camTransform.Position + Tbx::Vector3::Right() * _camMoveSpeed * deltaTime;
    }

    if (Tbx::Input::IsKeyDown(TBX_KEY_Q))
    {
		camTransform.Position = camTransform.Position + Tbx::Vector3::Forward() * _camMoveSpeed * deltaTime;
    }
    else if (Tbx::Input::IsKeyDown(TBX_KEY_E))
    {
		camTransform.Position = camTransform.Position + Tbx::Vector3::Backward() * _camMoveSpeed * deltaTime;
    }

	// Camera rotation
    if (Tbx::Input::IsKeyDown(TBX_KEY_LEFT))
    {
		auto rotationToApply = Tbx::Quaternion::FromAxisAngle(Tbx::Vector3(0.0f, 1.0f, 0.0f), -1.0f * _camRotateSpeed * deltaTime);
		camTransform.Rotation = camTransform.Rotation * rotationToApply;
    }
    else if (Tbx::Input::IsKeyDown(TBX_KEY_RIGHT))
    {
		auto rotationToApply = Tbx::Quaternion::FromAxisAngle(Tbx::Vector3(0.0f, 1.0f, 0.0f), 1.0f * _camRotateSpeed * deltaTime);
		camTransform.Rotation = camTransform.Rotation * rotationToApply;
    }
	else if (Tbx::Input::IsKeyDown(TBX_KEY_UP))
	{
		auto rotationToApply = Tbx::Quaternion::FromAxisAngle(Tbx::Vector3(1.0f, 0.0f, 0.0f), 1.0f * _camRotateSpeed * deltaTime);
		camTransform.Rotation = camTransform.Rotation * rotationToApply;
	}
	else if (Tbx::Input::IsKeyDown(TBX_KEY_DOWN))
	{
		auto rotationToApply = Tbx::Quaternion::FromAxisAngle(Tbx::Vector3(1.0f, 0.0f, 0.0f), -1.0f * _camRotateSpeed * deltaTime);
		camTransform.Rotation = camTransform.Rotation * rotationToApply;
	}
}
