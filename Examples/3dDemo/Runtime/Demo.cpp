#include "Demo.h"
#include <Tbx/Input/Input.h>
#include <Tbx/Input/InputCodes.h>
#include <Tbx/Graphics/Texture.h>
#include <Tbx/Graphics/Material.h>
#include <Tbx/Graphics/Camera.h>
#include <Tbx/Graphics/Mesh.h>
#include <Tbx/Math/Transform.h>
#include <Tbx/Math/Trig.h>
#include <Tbx/Events/TSSEvents.h>
#include <Tbx/Time/DeltaTime.h>
#include <vector>

Demo::Demo(const Tbx::WeakRef<Tbx::App>& app)
    : Runtime("3d Demo Runtime")
    , _app(app)
{
}

void Demo::OnStart()
{
    TBX_TRACE("Demo started!");

    // Load assets
    const auto& assetServer = _app.lock()->GetAssetServer();
    auto smilyTex = assetServer.GetAsset<Tbx::Texture>("Smily.png");
    auto wallTex = assetServer.GetAsset<Tbx::Texture>("Wall.jpg");
    auto checkerTex = assetServer.GetAsset<Tbx::Texture>("Checkerboard.png");
    auto fragmentShader = assetServer.GetAsset<Tbx::Shader>("fragment.frag");
    auto vertexShader = assetServer.GetAsset<Tbx::Shader>("vertex.vert");

    // Setup testing scene...
    _world = std::make_shared<Tbx::Stage>();
    auto worldRoot = _world->GetRoot();

    // Setup base material
    auto matShaders = { vertexShader, fragmentShader };
    _simpleTexturedMat = std::make_shared<Tbx::Material>(matShaders);

    // Create room
    {
        auto floor = std::make_shared<Tbx::Toy>("Floor");
        floor->Blocks.Add<Tbx::Mesh>();
        floor->Blocks.Add<Tbx::MaterialInstance>(_simpleTexturedMat, checkerTex);
        floor->Blocks.Add<Tbx::Transform>()
            .SetPosition({ 0, -25, 100 })
            .SetRotation(Tbx::Quaternion::FromEuler({ 90, 0, 0 }))
            .SetScale({ 50 });
        _world->GetRoot()->Children.push_back(floor);

        auto wallBack = std::make_shared<Tbx::Toy>("Wall Back");
        wallBack->Blocks.Add<Tbx::Mesh>();
        wallBack->Blocks.Add<Tbx::MaterialInstance>(_simpleTexturedMat, wallTex);
        wallBack->Blocks.Add<Tbx::Transform>()
            .SetPosition({ 0, 0, 125 })
            .SetRotation(Tbx::Quaternion::FromEuler({ 0, 0, 0 }))
            .SetScale({ 50 });
        _world->GetRoot()->Children.push_back(wallBack);

        auto wallRight = std::make_shared<Tbx::Toy>("Wall Right");
        wallRight->Blocks.Add<Tbx::Mesh>();
        wallRight->Blocks.Add<Tbx::MaterialInstance>(_simpleTexturedMat, wallTex);
        wallRight->Blocks.Add<Tbx::Transform>()
            .SetPosition({ 25, 0, 100 })
            .SetRotation(Tbx::Quaternion::FromEuler({ 0, -90, 0 }))
            .SetScale({ 50 });
        _world->GetRoot()->Children.push_back(wallRight);
    }

    // Create smily
    {
        auto smily = std::make_shared<Tbx::Toy>("Smily");
        smily->Blocks.Add<Tbx::Mesh>();
        smily->Blocks.Add<Tbx::MaterialInstance>(_simpleTexturedMat, smilyTex);
        smily->Blocks.Add<Tbx::Transform>()
            .SetPosition({ 0, 0, 100 })
            .SetRotation(Tbx::Quaternion::FromEuler({ 0, 0, 0 }))
            .SetScale({ 10 });
        _smily = smily;
        _world->GetRoot()->Children.push_back(_smily);
    }

    // Create camera
    {
        auto fpsCam = std::make_shared<Tbx::Toy>("Camera");
        fpsCam->Blocks.Add<Tbx::Camera>();
        fpsCam->Blocks.Add<Tbx::Transform>();
        _fpsCam = fpsCam;
        _world->GetRoot()->Children.push_back(_fpsCam);
    }

    // TODO: Figure out a better way than just needing to know you have to send this event...
    // Perhaps a stage manager/director?
    _app.lock()->GetEventBus()
        .Post(Tbx::StageOpenedEvent(_world));
}

void Demo::OnShutdown()
{
    TBX_TRACE("Demo shutdown!");
}

void Demo::OnUpdate()
{
    auto worldRoot = _world->GetRoot();
    const auto& deltaTime = Tbx::Time::DeltaTime::InSeconds();

    // Camera movement
    {
        auto& camTransform = _fpsCam->Blocks.Get<Tbx::Transform>();

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
            Tbx::Vector3 camMoveDir = Tbx::Vector3::Zero;

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
                    camMoveDir += Tbx::Vector3::Up;
                if (Tbx::Input::IsKeyHeld(TBX_KEY_Q) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_LEFT_BUMPER))
                    camMoveDir += Tbx::Vector3::Down;
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
            Tbx::Quaternion qPitch = Tbx::Quaternion::FromAxisAngle(Tbx::Vector3::Right, _camPitch);
            Tbx::Quaternion qYaw = Tbx::Quaternion::FromAxisAngle(Tbx::Vector3::Up, _camYaw);

            // Combine (usually yaw * pitch for FPS)
            camTransform.Rotation = Tbx::Quaternion::Normalize(qYaw * qPitch);
        }

    }

    // Smily movement
    {
        // rotate over time
        const float smilyRotateSpeed = 90.0f;
        auto& smilyTransform = _smily->Blocks.Get<Tbx::Transform>();
        float angle = Tbx::Math::PI * deltaTime * smilyRotateSpeed;
        Tbx::Quaternion qYaw = Tbx::Quaternion::FromAxisAngle(Tbx::Vector3::Up, angle);
        smilyTransform.Rotation = Tbx::Quaternion::Normalize(smilyTransform.Rotation * qYaw);

        // Bob over time
        const float smilyBobFrequency = 2;
        const float smilyBobScale = 1;
        _smilyBobTime += deltaTime * smilyBobFrequency;
        _smilyBobAmplitude = std::sin(_smilyBobTime);
        smilyTransform.Position.Y = (_smilyBobAmplitude * smilyBobScale);
    }
}
