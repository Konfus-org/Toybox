#include "ExampleLayer.h"
#include <Tbx/App/App.h>
#include <Tbx/Assets/Asset.h>
#include <Tbx/Input/Input.h>
#include <Tbx/Input/InputCodes.h>
#include <Tbx/Graphics/Texture.h>
#include <Tbx/Graphics/Material.h>
#include <Tbx/Graphics/Camera.h>
#include <Tbx/Graphics/Mesh.h>
#include <Tbx/Math/Transform.h>
#include <Tbx/Time/DeltaTime.h>
#include <algorithm>
#include <cmath>

void ExampleLayer::OnAttach()
{
    TBX_TRACE("Test scene attached!");

    // Load assets
    auto smilyTexAsset = Tbx::Asset<Tbx::Texture>("Assets/Smily.png");
    auto wallTexAsset = Tbx::Asset<Tbx::Texture>("Assets/Wall.jpg");
    auto checkerboardTexAsset = Tbx::Asset<Tbx::Texture>("Assets/Checkerboard.png");
    auto fragmentShaderAsset = Tbx::Asset<Tbx::Shader>("Assets/fragment.frag");
    auto vertexShaderAsset = Tbx::Asset<Tbx::Shader>("Assets/vertex.vert");

    // Setup testing scene...
    _world = GetApp()->GetLayer<Tbx::WorldLayer>();
    auto worldRoot = _world->GetRoot();

    // Setup base material
    auto vertShader = *vertexShaderAsset.GetData();
    auto fragShader = *fragmentShaderAsset.GetData();
    _simpleTexturedMat = Tbx::Material({vertShader, fragShader});

    // Get textures to use for our material instances
    auto smilyTex = *smilyTexAsset.GetData();
    auto wallTex = *wallTexAsset.GetData();
    auto checkerTex = *checkerboardTexAsset.GetData();

    // Create room
    {
        auto floor = worldRoot->EmplaceChild("Floor");
        floor->EmplaceBlock<Tbx::Mesh>();
        floor->EmplaceBlock<Tbx::MaterialInstance>(_simpleTexturedMat, checkerTex);
        floor->EmplaceBlock<Tbx::Transform>()
            .SetPosition({ 0, -25, 100 })
            .SetRotation(Tbx::Quaternion::FromEuler({ 90, 0, 0 }))
            .SetScale({ 50 });

        auto wallBack = worldRoot->EmplaceChild("Wall Back");
        wallBack->EmplaceBlock<Tbx::Mesh>();
        wallBack->EmplaceBlock<Tbx::MaterialInstance>(_simpleTexturedMat, wallTex);
        wallBack->EmplaceBlock<Tbx::Transform>()
            .SetPosition({ 0, 0, 125 })
            .SetRotation(Tbx::Quaternion::FromEuler({ 0, 0, 0 }))
            .SetScale({ 50 });

        auto wallRight = worldRoot->EmplaceChild("Wall Right");
        wallRight->EmplaceBlock<Tbx::Mesh>();
        wallRight->EmplaceBlock<Tbx::MaterialInstance>(_simpleTexturedMat, wallTex);
        wallRight->EmplaceBlock<Tbx::Transform>()
            .SetPosition({ 25, 0, 100 })
            .SetRotation(Tbx::Quaternion::FromEuler({ 0, -90, 0 }))
            .SetScale({ 50 });
    }

    // Create smily
    {
        auto smily = worldRoot->EmplaceChild("Smily");
        smily->EmplaceBlock<Tbx::Mesh>();
        smily->EmplaceBlock<Tbx::MaterialInstance>(_simpleTexturedMat, smilyTex);
        smily->EmplaceBlock<Tbx::Transform>()
            .SetPosition({ 0, 0, 100 })
            .SetRotation(Tbx::Quaternion::FromEuler({ 0, 0, 0 }))
            .SetScale({ 10 });
        _smily = smily;
    }

    // Create camera
    {
        auto fpsCam = worldRoot->EmplaceChild("Camera");
        fpsCam->EmplaceBlock<Tbx::Camera>();
        fpsCam->EmplaceBlock<Tbx::Transform>();
        _fpsCam = fpsCam;
    }
}

void ExampleLayer::OnDetach()
{
    TBX_TRACE("Test scene detached!");
}

void ExampleLayer::OnUpdate()
{
    auto worldRoot = _world->GetRoot();
    const auto& deltaTime = Tbx::Time::DeltaTime::InSeconds();

    // Camera movement
    {
        auto& camTransform = _fpsCam->GetBlock<Tbx::Transform>();

        // Determine movement speed
        const float camMoveSpeed = 10.0f;
        float camSpeed = camMoveSpeed;
        if (Tbx::Input::IsKeyHeld(TBX_KEY_LEFT_SHIFT) ||
            Tbx::Input::IsKeyHeld(TBX_KEY_RIGHT_SHIFT) ||
            Tbx::Input::GetGamepadAxis(0, TBX_GAMEPAD_AXIS_LEFT_TRIGGER) > TBX_GAMEPAD_AXIS_DEADZONE)
        {
            camSpeed *= 5.0f;
        }

        // Get movement dir
        {
            Tbx::Vector3 camMoveDir = Tbx::Consts::Vector3::Zero;

            // Get WASD/dpad style
            {
                if (Tbx::Input::IsKeyHeld(TBX_KEY_W) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_DPAD_UP))
                    camMoveDir += Tbx::Quaternion::GetForward(camTransform.Rotation);
                if (Tbx::Input::IsKeyHeld(TBX_KEY_S) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_DPAD_DOWN))
                    camMoveDir -= Tbx::Quaternion::GetForward(camTransform.Rotation);
                if (Tbx::Input::IsKeyHeld(TBX_KEY_D) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_DPAD_RIGHT))
                    camMoveDir -= Tbx::Quaternion::GetRight(camTransform.Rotation);
                if (Tbx::Input::IsKeyHeld(TBX_KEY_A) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_DPAD_LEFT))
                    camMoveDir += Tbx::Quaternion::GetRight(camTransform.Rotation);
                if (Tbx::Input::IsKeyHeld(TBX_KEY_E) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_RIGHT_BUMPER))
                    camMoveDir += Tbx::WorldSpace::Up;
                if (Tbx::Input::IsKeyHeld(TBX_KEY_Q) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_LEFT_BUMPER))
                    camMoveDir += Tbx::WorldSpace::Down;
            }
            // Get controller axis style
            {
                auto leftStickXAxisValue = Tbx::Input::GetGamepadAxis(0, TBX_GAMEPAD_AXIS_LEFT_X);
                auto leftStickYAxisValue = -Tbx::Input::GetGamepadAxis(0, TBX_GAMEPAD_AXIS_LEFT_Y);

                if (leftStickXAxisValue > TBX_GAMEPAD_AXIS_DEADZONE  ||
                    leftStickXAxisValue < -TBX_GAMEPAD_AXIS_DEADZONE)
                {
                    camMoveDir -= Tbx::Quaternion::GetRight(camTransform.Rotation) * Tbx::Vector3(leftStickXAxisValue, 0, 0).Normalize();
                }
                if (leftStickYAxisValue > TBX_GAMEPAD_AXIS_DEADZONE ||
                    leftStickYAxisValue < -TBX_GAMEPAD_AXIS_DEADZONE)
                {
                    camMoveDir += Tbx::Quaternion::GetForward(camTransform.Rotation) * Tbx::Vector3(0, 0, leftStickYAxisValue).Normalize();
                }
            }

            // Apply movement if any
            if (!camMoveDir.IsNearlyZero())
            {
                camTransform.Position += camMoveDir.Normalize() * camSpeed * deltaTime;
            }
        }

        // Camera rotation
        {
            const float camRotateSpeed = 180.0f;

            // Arrow and gamepad btn style
            {
                if (Tbx::Input::IsKeyHeld(TBX_KEY_LEFT) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_WEST))
                    _camYaw -= camRotateSpeed * deltaTime;
                if (Tbx::Input::IsKeyHeld(TBX_KEY_RIGHT) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_EAST))
                    _camYaw += camRotateSpeed * deltaTime;
                if (Tbx::Input::IsKeyHeld(TBX_KEY_UP) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_NORTH))
                    _camPitch += camRotateSpeed * deltaTime;
                if (Tbx::Input::IsKeyHeld(TBX_KEY_DOWN) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_SOUTH))
                    _camPitch -= camRotateSpeed * deltaTime;
            }
            // Mouse and gamepad axis style
            {
                auto mouseDelta = Tbx::Input::GetMouseDelta();
                if (Tbx::Input::IsMouseButtonHeld(TBX_MOUSE_BUTTON_RIGHT))
                {
                    if (mouseDelta.X != 0)
                    {
                        _camYaw += mouseDelta.X * (camRotateSpeed / 5) * deltaTime;
                    }
                    if (mouseDelta.Y != 0)
                    {
                        _camPitch -= mouseDelta.Y * (camRotateSpeed / 5) * deltaTime;
                    }
                }

                auto rightStickXAxisValue = Tbx::Input::GetGamepadAxis(0, TBX_GAMEPAD_AXIS_RIGHT_X);
                auto rightStickYAxisValue = -Tbx::Input::GetGamepadAxis(0, TBX_GAMEPAD_AXIS_RIGHT_Y);
                if (rightStickXAxisValue > TBX_GAMEPAD_AXIS_DEADZONE  ||
                    rightStickXAxisValue < -TBX_GAMEPAD_AXIS_DEADZONE ||
                    rightStickYAxisValue > TBX_GAMEPAD_AXIS_DEADZONE  ||
                    rightStickYAxisValue < -TBX_GAMEPAD_AXIS_DEADZONE)
                {
                    _camYaw += rightStickXAxisValue * camRotateSpeed * deltaTime;
                    _camPitch += rightStickYAxisValue * camRotateSpeed * deltaTime;
                }
            }

            // Clamp pitch to avoid flipping
            _camPitch = std::clamp(_camPitch, -89.0f, 89.0f);

            // Build rotation
            Tbx::Quaternion qPitch = Tbx::Quaternion::FromAxisAngle(Tbx::WorldSpace::Right, _camPitch);
            Tbx::Quaternion qYaw = Tbx::Quaternion::FromAxisAngle(Tbx::WorldSpace::Up, _camYaw);

            // Combine (usually yaw * pitch for FPS)
            camTransform.Rotation = Tbx::Quaternion::Normalize(qYaw * qPitch);
        }

    }

    // Smily movement
    {
        // rotate over time
        const float smilyRotateSpeed = 90.0f;
        auto& smilyTransform = _smily->GetBlock<Tbx::Transform>();
        float angle = Tbx::Consts::PI * deltaTime * smilyRotateSpeed;
        Tbx::Quaternion qYaw = Tbx::Quaternion::FromAxisAngle(Tbx::WorldSpace::Up, angle);
        smilyTransform.Rotation = Tbx::Quaternion::Normalize(smilyTransform.Rotation * qYaw);

        // Bob over time
        const float smilyBobFrequency = 2;
        const float smilyBobScale = 1;
        _smilyBobTime += deltaTime * smilyBobFrequency;
        _smilyBobAmplitude = std::sin(_smilyBobTime);
        smilyTransform.Position.Y = (_smilyBobAmplitude * smilyBobScale);
    }
}
