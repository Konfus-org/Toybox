#include "TestSceneLayer.h"
#include <Tbx/Core/Rendering/RenderingAPI.h>
#include <Tbx/Runtime/App.h>
#include <Tbx/Runtime/Input/Input.h>
#include <Tbx/Runtime/Input/InputCodes.h>
#include <Tbx/Runtime/Time/DeltaTime.h>
#include <Tbx/Runtime/Windowing/WindowManager.h>
#include <Tbx/Runtime/Render Pipeline/RenderPipeline.h>
#include <Tbx/Core/TBS/Toy.h>
#include <Tbx/Core/Math/Transform.h>
#include <Tbx/Core/Rendering/DefaultShader.h>

void TestSceneLayer::OnAttach()
{
	TBX_TRACE("Test scene attached!");

	// Setup testing scene...
    _playSpace = std::make_shared<Tbx::Playspace>();

    auto rootBox = _playSpace->AddBox();
	auto checkerBox2dToy = rootBox->AddToy();

    // Add mesh and transform blocks to the toy
    checkerBox2dToy->AddBlock<Tbx::Mesh>();
    checkerBox2dToy->AddBlock<Tbx::Transform>();

	// Add a material to the toy
    auto material = checkerBox2dToy->AddBlock<Tbx::Material>();
    material->SetTexture(0, Tbx::Texture("Assets/Checkerboard.png"));

	//Tbx::RenderPipeline::SetContext(_playSpace)
}

void TestSceneLayer::OnDetach()
{
	TBX_TRACE("Test scene detached!");
}

void TestSceneLayer::OnUpdate()
{
	//if (Tbx::Input::IsKeyDown(TBX_KEY_SPACE)) TBX_TRACE("Space pressed!");

	const auto& deltaTime = Tbx::Time::DeltaTime::Seconds();
	//TBX_TRACE("Delta Time: {0}", deltaTime);

	// Camera movement and rotation
	//if (Tbx::Input::IsKeyDown(TBX_KEY_W))
	//{
	//	mainCam->SetPosition(mainWindowCam->GetPosition() + (Tbx::Vector3::Up() * _camMoveSpeed * Tbx::Time::DeltaTime::Seconds()));
	//}
 //   else if (Tbx::Input::IsKeyDown(TBX_KEY_S))
 //   {
 //       mainCam->SetPosition(mainWindowCam->GetPosition() + (Tbx::Vector3::Down() * _camMoveSpeed * Tbx::Time::DeltaTime::Seconds()));
 //   }

 //   if (Tbx::Input::IsKeyDown(TBX_KEY_A))
 //   {
 //       mainCam->SetPosition(mainWindowCam->GetPosition() + (Tbx::Vector3::Left() * _camMoveSpeed) * Tbx::Time::DeltaTime::Seconds());
 //   }
 //   else if (Tbx::Input::IsKeyDown(TBX_KEY_D))
 //   {
 //       mainCam->SetPosition(mainWindowCam->GetPosition() + (Tbx::Vector3::Right() * _camMoveSpeed * Tbx::Time::DeltaTime::Seconds()));
 //   }

 //   if (Tbx::Input::IsKeyDown(TBX_KEY_UP))
 //   {
 //       mainCam->SetPosition(mainWindowCam->GetPosition() + (Tbx::Vector3::Forward() * _camMoveSpeed * Tbx::Time::DeltaTime::Seconds()));
 //   }
 //   else if (Tbx::Input::IsKeyDown(TBX_KEY_DOWN))
 //   {
 //       mainCam->SetPosition(mainWindowCam->GetPosition() + (Tbx::Vector3::Backward() * _camMoveSpeed * Tbx::Time::DeltaTime::Seconds()));
 //   }

 //   if (Tbx::Input::IsKeyDown(TBX_KEY_Q))
 //   {
	//	const auto& currentRot = mainWindowCam->GetRotation();
	//	const auto& newRot = currentRot * (Tbx::Vector3(0.0f, 0.0f, -1.0f) * _camRotateSpeed * Tbx::Time::DeltaTime::Seconds());
 //       mainCam->SetRotation(newRot);
 //   }
 //   else if (Tbx::Input::IsKeyDown(TBX_KEY_E))
 //   {
	//	const auto& currentRot = mainWindowCam->GetRotation();
	//	const auto& newRot = currentRot * (Tbx::Vector3(0.0f, 0.0f, 1.0f) * _camRotateSpeed * Tbx::Time::DeltaTime::Seconds());
 //       mainCam->SetRotation(newRot);
 //   }

	// Triangle movement
	////if (Tbx::Input::IsKeyDown(TBX_KEY_SPACE))
	////{
	////	_trianglePosition = _trianglePosition + (Tbx::Vector3::Up() * _camMoveSpeed * 2 * Tbx::Time::DeltaTime::Seconds());
	////}

	////DrawSquareTest();

	//TBX_TRACE("Camera Position: {0}", mainWindowCam->GetPosition().ToString());

	const auto& shaderData = Tbx::ShaderData("viewProjection", mainWindowCam->GetViewProjectionMatrix(), Tbx::ShaderDataType::Mat4);
	const auto& renderData = Tbx::RenderData(Tbx::RenderCommand::UploadShaderData, shaderData);
    Tbx::RenderPipeline::Push(renderData);
}
