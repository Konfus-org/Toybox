#include "ExampleLayer.h"
#include <Tbx/Input/Input.h>
#include <Tbx/Input/InputCodes.h>
#include <Tbx/TBS/Toy.h>
#include <Tbx/TBS/World.h>
#include <Tbx/Graphics/Texture.h>
#include <Tbx/Graphics/Material.h>
#include <Tbx/Graphics/Camera.h>
#include <Tbx/Graphics/Mesh.h>
#include <Tbx/Math/Transform.h>
#include <Tbx/Time/DeltaTime.h>
#include <algorithm>

void ExampleLayer::OnAttach()
{
	TBX_TRACE("Test scene attached!");

	// Setup testing scene...
    auto playSpaceId = Tbx::World::MakeBox();
	_level = Tbx::World::GetPlayspace(playSpaceId);

    // Create blue toy
	//auto checkerText = Tbx::Texture("Assets/Checkerboard.png");
	auto checkerText = Tbx::Texture();
	Tbx::Toy blueToy = _level->MakeToy("Checkerboard");
	_level->AddBlockTo<Tbx::Mesh>(blueToy);
	_level->AddBlockTo<Tbx::Transform>(blueToy);
	auto& blueMat = _level->AddBlockTo<Tbx::Material>(blueToy);

	// Create background checkerboard
	Tbx::Toy checkerBoxToyF = _level->MakeToy("Checkerboard F");
	_level->AddBlockTo<Tbx::Mesh>(checkerBoxToyF);
	auto& checkerTrans = _level->AddBlockTo<Tbx::Transform>(checkerBoxToyF);
	checkerTrans.Position = { 0, 0, 100 };
	checkerTrans.Scale = { 1000 };
	auto& checkerMat = _level->AddBlockTo<Tbx::Material>(checkerBoxToyF);
	checkerMat.SetTexture(0, checkerText);

	Tbx::Toy checkerBoxToyL = _level->MakeToy("Checkerboard L");
	_level->AddBlockTo<Tbx::Mesh>(checkerBoxToyL);
	auto& checkerTransL = _level->AddBlockTo<Tbx::Transform>(checkerBoxToyL);
	checkerTransL.Position = { -100, 0, 0 };
	checkerTransL.Rotation = Tbx::Quaternion::FromEuler({ 0, 90, 0 });
	checkerTransL.Scale = { 1000 };
	auto& checkerMatL = _level->AddBlockTo<Tbx::Material>(checkerBoxToyL);
	checkerMatL.SetTexture(0, checkerText);

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
	const auto& deltaTime = Tbx::Time::DeltaTime::InSeconds();

	// Camera movement
	auto& camTransform = _level->GetBlockOn<Tbx::Transform>(_fpsCam);

	// Determine movement speed
	float camSpeed = _camMoveSpeed * deltaTime;
	/*if (Tbx::Input::IsKeyDown(TBX_KEY_LEFT_SHIFT))
	{
		camSpeed *= 4.0f;
	}*/

	// Build movement direction
	Tbx::Vector3 camMoveDir = Tbx::Constants::Vector3::Zero;
	/*if (Tbx::Input::IsKeyDown(TBX_KEY_W)) camMoveDir += Tbx::Quaternion::GetForward(camTransform.Rotation);
	if (Tbx::Input::IsKeyDown(TBX_KEY_S)) camMoveDir -= Tbx::Quaternion::GetForward(camTransform.Rotation);

	if (Tbx::Input::IsKeyDown(TBX_KEY_D)) camMoveDir -= Tbx::Quaternion::GetRight(camTransform.Rotation);
	if (Tbx::Input::IsKeyDown(TBX_KEY_A)) camMoveDir += Tbx::Quaternion::GetRight(camTransform.Rotation);

	if (Tbx::Input::IsKeyDown(TBX_KEY_E)) camMoveDir += Tbx::WorldSpace::Up;
	if (Tbx::Input::IsKeyDown(TBX_KEY_Q)) camMoveDir += Tbx::WorldSpace::Down;*/

	// Apply movement if any
	if (!camMoveDir.IsNearlyZero())
	{
		camTransform.Position += camMoveDir.Normalize() * camSpeed;
	}

	// Camera rotation
	/*if (Tbx::Input::IsKeyDown(TBX_KEY_LEFT))  _camYaw -= _camRotateSpeed * deltaTime;
	if (Tbx::Input::IsKeyDown(TBX_KEY_RIGHT)) _camYaw += _camRotateSpeed * deltaTime;
	if (Tbx::Input::IsKeyDown(TBX_KEY_UP))    _camPitch += _camRotateSpeed * deltaTime;
	if (Tbx::Input::IsKeyDown(TBX_KEY_DOWN))  _camPitch -= _camRotateSpeed * deltaTime;*/

	// Clamp pitch to avoid flipping
	_camPitch = std::clamp(_camPitch, -89.0f, 89.0f);

	// Build rotation
	Tbx::Quaternion qPitch = Tbx::Quaternion::FromAxisAngle(Tbx::WorldSpace::Right, _camPitch);
	Tbx::Quaternion qYaw = Tbx::Quaternion::FromAxisAngle(Tbx::WorldSpace::Up, _camYaw);

	// Combine (usually yaw * pitch for FPS)
	camTransform.Rotation = Tbx::Quaternion::Normalize(qYaw * qPitch);

}
