#include "ExampleLayer.h"
#include <Tbx/Assets/Asset.h>
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

Tbx::Material _stdMat;

void ExampleLayer::OnAttach()
{
    TBX_TRACE("Test scene attached!");

    // Load assets
    auto wallTexAsset = Tbx::Asset<Tbx::Texture>("../Examples/Simple App/Assets/Wall.jpg");
    auto checkerboardTexAsset = Tbx::Asset<Tbx::Texture>("../Examples/Simple App/Assets/Checkerboard.png");
    auto fragmentShaderAsset = Tbx::Asset<Tbx::Shader>("../Examples/Simple App/Assets/fragment.frag");
    auto vertexShaderAsset = Tbx::Asset<Tbx::Shader>("../Examples/Simple App/Assets/vertex.vert");

    // Setup testing scene...
    auto boxId = Tbx::World::MakeBox();
    _level = Tbx::World::GetBox(boxId);

    // Setup checker material
    auto vertShader = *vertexShaderAsset.GetData();
    auto fragShader = *fragmentShaderAsset.GetData();
    auto checkerTex = *checkerboardTexAsset.GetData();
    auto wallTex = *wallTexAsset.GetData();
    _stdMat = Tbx::Material({vertShader, fragShader});

    // Create room
    {
        Tbx::ToyHandle floor = _level->EmplaceToy("Floor");
        _level->EmplaceBlockOn<Tbx::Mesh>(floor);
        _level->EmplaceBlockOn<Tbx::MaterialInstance>(floor, _stdMat, checkerTex);
        _level->EmplaceBlockOn<Tbx::Transform>(floor)
            .SetPosition({ 0, -25, 100 })
            .SetRotation(Tbx::Quaternion::FromEuler({ 90, 0, 0 }))
            .SetScale({ 50 });
        Tbx::ToyHandle wallBack = _level->EmplaceToy("Wall Back");
        _level->EmplaceBlockOn<Tbx::Mesh>(wallBack);
        _level->EmplaceBlockOn<Tbx::MaterialInstance>(wallBack, _stdMat, wallTex);
        _level->EmplaceBlockOn<Tbx::Transform>(wallBack)
            .SetPosition({ 0, 0, 125 })
            .SetRotation(Tbx::Quaternion::FromEuler({ 0, 0, 0 }))
            .SetScale({ 50 });
        Tbx::ToyHandle wallRight = _level->EmplaceToy("Wall Right");
        _level->EmplaceBlockOn<Tbx::Mesh>(wallRight);
        _level->EmplaceBlockOn<Tbx::MaterialInstance>(wallRight, _stdMat, wallTex);
        _level->EmplaceBlockOn<Tbx::Transform>(wallRight)
            .SetPosition({ 25, 0, 100 })
            .SetRotation(Tbx::Quaternion::FromEuler({ 0, -90, 0 }))
            .SetScale({ 50 });
    }

    // Create camera
    {
        _fpsCam = _level->EmplaceToy("Camera");
        _level->EmplaceBlockOn<Tbx::Camera>(_fpsCam);
        auto& camTransform = _level->EmplaceBlockOn<Tbx::Transform>(_fpsCam);
    }

    // Open our new level
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
    if (Tbx::Input::IsKeyDown(TBX_KEY_LEFT_SHIFT))
    {
        camSpeed *= 5.0f;
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
