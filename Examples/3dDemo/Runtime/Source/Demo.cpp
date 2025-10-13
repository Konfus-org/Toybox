#include "Demo.h"
#include <Tbx/Input/Input.h>
#include <Tbx/Input/InputCodes.h>
#include <Tbx/Graphics/Texture.h>
#include <Tbx/Graphics/Material.h>
#include <Tbx/Graphics/Camera.h>
#include <Tbx/Graphics/Mesh.h>
#include <Tbx/Math/Transform.h>
#include <Tbx/Math/Trig.h>
#include <Tbx/Time/DeltaTime.h>
#include <Tbx/Events/StageEvents.h>
#include <algorithm>
#include <cmath>

Demo::Demo(Tbx::Ref<Tbx::EventBus> eventBus)
{
    TBX_TRACE_INFO("Demo: loaded!\n");
}

Demo::~Demo()
{
    TBX_TRACE_INFO("Demo: unloaded!\n");
}

void Demo::OnStart()
{
    TBX_TRACE_INFO("Demo: started!\n");

    // Load assets
    auto music = Assets->Get<Tbx::Audio>("Music.wav");
    auto wallTex = Assets->Get<Tbx::Texture>("Wall.jpg");
    auto floorTex = Assets->Get<Tbx::Texture>("Checkerboard.png");
    auto smilyTex = Assets->Get<Tbx::Texture>("Smily.png");
    auto fragmentShader = Assets->Get<Tbx::Shader>("Fragment.frag");
    auto vertexShader = Assets->Get<Tbx::Shader>("Vertex.vert");
    auto matShaders = { vertexShader, fragmentShader };

    // Setup testing scene:
    {
        _stage = Tbx::Stage::Make();
        auto worldRoot = _stage->Root;

        // Add audio
        auto musicPlayer = _stage->Add("MusicPlayer");
        auto musicSource = musicPlayer->Add<Tbx::AudioSource>(music);
        musicSource->Playing = true;
        musicSource->Looping = true;
        musicSource->PlaybackSpeed = 3;
        musicSource->Pitch = 0.25f;

        // We need to have at least once instance of our mesh and materials to be able to instance them
        // TODO: This is a bit of a hack, but it works for now.
        auto quadToy = _stage->Add("Quad");
        auto quadId = quadToy->Add<Tbx::Mesh>()->Id;

        auto wallMatToy = _stage->Add("WallMat");
        auto wallMatId = wallMatToy->Add<Tbx::Material>(matShaders, wallTex)->Id;

        auto floorMatToy = _stage->Add("FloorMat");
        auto floorMatId = floorMatToy->Add<Tbx::Material>(matShaders, floorTex)->Id;

        auto smilyMatToy = _stage->Add("SmilyMat");
        auto smilyMatId = smilyMatToy->Add<Tbx::Material>(matShaders, smilyTex)->Id;

        // Add music

        // Create room
        {
            auto wallBack = _stage->Add("Wall Back");
            wallBack->Add<Tbx::MeshInstance>(quadId);
            wallBack->Add<Tbx::MaterialInstance>(wallMatId);
            if (auto transform = wallBack->Add<Tbx::Transform>())
            {
                transform->SetPosition({ 0, 0, 125 });
                transform->SetRotation(Tbx::Quaternion::FromEuler({ 0, 0, 0 }));
                transform->SetScale({ 50 });
            }

            auto wallRight = _stage->Add("Wall Right");
            wallRight->Add<Tbx::MeshInstance>(quadId);
            wallRight->Add<Tbx::MaterialInstance>(wallMatId);
            if (auto transform = wallRight->Add<Tbx::Transform>())
            {
                transform->SetPosition({ 25, 0, 100 });
                transform->SetRotation(Tbx::Quaternion::FromEuler({ 0, -90, 0 }));
                transform->SetScale({ 50 });
            }

            auto floor = _stage->Add("Floor");
            floor->Add<Tbx::MeshInstance>(quadId);
            floor->Add<Tbx::MaterialInstance>(floorMatId);
            if (auto transform = floor->Add<Tbx::Transform>())
            {
                transform->SetPosition({ 0, -25, 100 });
                transform->SetRotation(Tbx::Quaternion::FromEuler({ 90, 0, 0 }));
                transform->SetScale({ 50 });
            }
        }

        // Create smily
        {
            _smilyBaseHeight = 0;
            _smily = _stage->Add("Smily");
            _smily->Add<Tbx::MeshInstance>(quadId);
            _smily->Add<Tbx::MaterialInstance>(smilyMatId);
            auto& transform = *_smily->Add<Tbx::Transform>();
            transform
                .SetPosition({ 0, _smilyBaseHeight, 100 })
                .SetRotation(Tbx::Quaternion::FromEuler({ 0, 0, 0 }))
                .SetScale({ 10 });
        }

        // Create camera
        {
            auto fpsCam = _stage->Add("Camera");
            fpsCam->Add<Tbx::Camera>();
            fpsCam->Add<Tbx::Transform>();
            _fpsCam = fpsCam;
        }
    }

    // TODO: Figure out a better way than just needing to know you have to send this event...
    // Perhaps a stage manager/director?
    Carrier.Post(Tbx::StageOpenedEvent(_stage));
}

void Demo::OnShutdown()
{
    TBX_TRACE_INFO("Demo: shutdown!\n");
}

void Demo::OnUpdate(const Tbx::DeltaTime& deltaTime)
{
    auto worldRoot = _stage->Root;
    const float deltaTimeSeconds = deltaTime.Seconds;

    // Camera movement
    {
        auto& camTransform = *_fpsCam->Get<Tbx::Transform>();

        // Camera rotation
        {
            const float camRotateSpeedDegPerSec = 180.0f; // for keys / gamepad (keep with deltaTime)
            const float mouseSensitivityDegPerPx = 0.35f; // tune to taste
            const float mouseSmoothingRate = 11.0f;       // higher == snappier response, lower == smoother

            // Arrow and gamepad btn style
            {
                if (Tbx::Input::IsKeyHeld(TBX_KEY_LEFT) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_WEST))
                    _camYaw -= camRotateSpeedDegPerSec * deltaTimeSeconds;
                if (Tbx::Input::IsKeyHeld(TBX_KEY_RIGHT) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_EAST))
                    _camYaw += camRotateSpeedDegPerSec * deltaTimeSeconds;
                if (Tbx::Input::IsKeyHeld(TBX_KEY_UP) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_NORTH))
                    _camPitch += camRotateSpeedDegPerSec * deltaTimeSeconds;
                if (Tbx::Input::IsKeyHeld(TBX_KEY_DOWN) || Tbx::Input::IsGamepadButtonHeld(0, TBX_GAMEPAD_BUTTON_SOUTH))
                    _camPitch -= camRotateSpeedDegPerSec * deltaTimeSeconds;
            }
            // Mouse and gamepad axis style
            {
                auto mouseDelta = Tbx::Input::GetMouseDelta();
                if (Tbx::Input::IsMouseButtonHeld(TBX_MOUSE_BUTTON_RIGHT))
                {
                    const Tbx::Vector2 targetLookDelta =
                        Tbx::Vector2(mouseDelta.X * mouseSensitivityDegPerPx,
                                     mouseDelta.Y * mouseSensitivityDegPerPx);

                    const float smoothingFactor = std::clamp(deltaTimeSeconds * mouseSmoothingRate, 0.0f, 1.0f);
                    _camLookVelocity += (targetLookDelta - _camLookVelocity) * smoothingFactor;

                    _camYaw += _camLookVelocity.X;
                    _camPitch -= _camLookVelocity.Y;
                }
                else
                {
                    _camLookVelocity = Tbx::Vector2::Zero;
                }

                auto rightStickXAxisValue = Tbx::Input::GetGamepadAxis(0, TBX_GAMEPAD_AXIS_RIGHT_X);
                auto rightStickYAxisValue = -Tbx::Input::GetGamepadAxis(0, TBX_GAMEPAD_AXIS_RIGHT_Y);
                if (rightStickXAxisValue > TBX_GAMEPAD_AXIS_DEADZONE  ||
                    rightStickXAxisValue < -TBX_GAMEPAD_AXIS_DEADZONE ||
                    rightStickYAxisValue > TBX_GAMEPAD_AXIS_DEADZONE  ||
                    rightStickYAxisValue < -TBX_GAMEPAD_AXIS_DEADZONE)
                {
                    _camYaw += rightStickXAxisValue * camRotateSpeedDegPerSec * deltaTimeSeconds;
                    _camPitch += rightStickYAxisValue * camRotateSpeedDegPerSec * deltaTimeSeconds;
                }
            }

            // Clamp pitch to avoid flipping
            _camPitch = std::clamp(_camPitch, -89.0f, 89.0f);
            _camYaw = std::fmod(_camYaw, 360.0f);
            if (_camYaw > 180.0f)
            {
                _camYaw -= 360.0f;
            }
            else if (_camYaw < -180.0f)
            {
                _camYaw += 360.0f;
            }

            // Build rotation
            Tbx::Quaternion qPitch = Tbx::Quaternion::FromAxisAngle(Tbx::Vector3::Right, _camPitch);
            Tbx::Quaternion qYaw = Tbx::Quaternion::FromAxisAngle(Tbx::Vector3::Up, _camYaw);

            // Combine (usually yaw * pitch for FPS)
            camTransform.Rotation = Tbx::Quaternion::Normalize(qYaw * qPitch);
        }

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
                camTransform.Position += camMoveDir.Normalize() * camSpeed * deltaTimeSeconds;
            }
        }
    }

    // Smily movement
    {
        // rotate over time
        const float smilyRotateSpeed = 90.0f;
        auto& smilyTransform = *_smily->Get<Tbx::Transform>();
        float angle = Tbx::PI * deltaTimeSeconds * smilyRotateSpeed;
        Tbx::Quaternion qYaw = Tbx::Quaternion::FromAxisAngle(Tbx::Vector3::Up, angle);
        smilyTransform.Rotation = Tbx::Quaternion::Normalize(smilyTransform.Rotation * qYaw);

        // Bob over time
        const float smilyBobFrequency = 2;
        const float smilyBobScale = 1;
        const float smilyBobAngularFrequency = smilyBobFrequency * Tbx::PI * 2.0f;
        _smilyBobTime += deltaTimeSeconds * smilyBobAngularFrequency;
        _smilyBobAmplitude = std::sin(_smilyBobTime);
        smilyTransform.Position.Y = _smilyBaseHeight + (_smilyBobAmplitude * smilyBobScale);
    }
}
