#include "ExampleLayer.h"
#include <Tbx/Core/Rendering/RenderingAPI.h>
#include <Tbx/Runtime/Input/Input.h>
#include <Tbx/Runtime/Input/InputCodes.h>
#include <Tbx/Runtime/Time/DeltaTime.h>
#include <Tbx/Core/Math/Transform.h>
#include <Tbx/Core/TBS/Toy.h>
#include <Tbx/Core/TBS/World.h>

void ExampleLayer::OnAttach()
{
	TBX_TRACE("Test scene attached!");

	// Setup testing scene...
    auto playSpaceId = Tbx::World::MakePlayspace();
	_level = Tbx::World::GetPlayspace(playSpaceId);

    // Create checkboard toy
	Tbx::Toy checkerBox2dToy = _level->MakeToy("Checkerboard");
	_level->AddBlockTo<Tbx::Mesh>(checkerBox2dToy);
	_level->AddBlockTo<Tbx::Transform>(checkerBox2dToy);
	auto& material = _level->AddBlockTo<Tbx::Material>(checkerBox2dToy);
    material.SetTexture(0, Tbx::Texture("Assets/Checkerboard.png"));
	material.SetColor(Tbx::Colors::Yellow);

	// Create camera toy
	_fpsCam = _level->MakeToy("Camera");
	_level->AddBlockTo<Tbx::Camera>(_fpsCam);
	auto& transform = _level->AddBlockTo<Tbx::Transform>(_fpsCam);
	transform.Position = { 0, 0, -10 };

	// Opens our new playSpace
	_level->Open();
}

void ExampleLayer::OnDetach()
{
	TBX_TRACE("Test scene detached!");
}

void ExampleLayer::OnUpdate()
{
	const auto& deltaTime = Tbx::Time::DeltaTime::Seconds();

	// Camera movement
	auto& camTransform = _level->GetBlockOn<Tbx::Transform>(_fpsCam);

	// Determine movement speed
	float camSpeed = _camMoveSpeed * deltaTime;
	if (Tbx::Input::IsKeyDown(TBX_KEY_LEFT_SHIFT))
	{
		camSpeed *= 4.0f;
	}

	// Build movement direction
	Tbx::Vector3 camMoveDir = Tbx::Constants::Vector3::Zero;
	if (Tbx::Input::IsKeyDown(TBX_KEY_W)) camMoveDir += Tbx::Quaternion::GetForward(camTransform.Rotation);
	if (Tbx::Input::IsKeyDown(TBX_KEY_S)) camMoveDir -= Tbx::Quaternion::GetForward(camTransform.Rotation);

	if (Tbx::Input::IsKeyDown(TBX_KEY_D)) camMoveDir -= Tbx::Quaternion::GetRight(camTransform.Rotation);
	if (Tbx::Input::IsKeyDown(TBX_KEY_A)) camMoveDir += Tbx::Quaternion::GetRight(camTransform.Rotation);

	if (Tbx::Input::IsKeyDown(TBX_KEY_E)) camMoveDir += Tbx::WorldSpace::Up;
	if (Tbx::Input::IsKeyDown(TBX_KEY_Q)) camMoveDir += Tbx::WorldSpace::Down;

	// Apply movement if any
	if (!camMoveDir.IsNearlyZero())
	{
		camTransform.Position += camMoveDir.Normalize() * camSpeed;
	}

	// Camera rotation
	if (Tbx::Input::IsKeyDown(TBX_KEY_LEFT))  _camYaw -= _camRotateSpeed * deltaTime;
	if (Tbx::Input::IsKeyDown(TBX_KEY_RIGHT)) _camYaw += _camRotateSpeed * deltaTime;
	if (Tbx::Input::IsKeyDown(TBX_KEY_UP))    _camPitch += _camRotateSpeed * deltaTime;
	if (Tbx::Input::IsKeyDown(TBX_KEY_DOWN))  _camPitch -= _camRotateSpeed * deltaTime;

	// Clamp pitch to avoid flipping
	_camPitch = std::clamp(_camPitch, -89.0f, 89.0f);

	// Build rotation
	Tbx::Quaternion qPitch = Tbx::Quaternion::FromAxisAngle(Tbx::WorldSpace::Right, _camPitch);
	Tbx::Quaternion qYaw = Tbx::Quaternion::FromAxisAngle(Tbx::WorldSpace::Up, _camYaw);

	// Combine (usually yaw * pitch for FPS)
	camTransform.Rotation = Tbx::Quaternion::Normalize(qYaw * qPitch);

}
