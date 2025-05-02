#include "TestSceneLayer.h"
#include <Tbx/Core/Rendering/RenderingAPI.h>
#include <Tbx/Runtime/Input/Input.h>
#include <Tbx/Runtime/Input/InputCodes.h>
#include <Tbx/Runtime/Time/DeltaTime.h>
#include <Tbx/Core/Math/Transform.h>
#include <Tbx/Core/TBS/Toy.h>
#include <Tbx/Core/TBS/World.h>

void TestSceneLayer::OnAttach()
{
	TBX_TRACE("Test scene attached!");

	// Setup testing scene...
    auto playSpaceId = Tbx::World::MakePlayspace();
	auto playspace = Tbx::World::GetPlayspace(playSpaceId);

    // Create checkboard toy
	Tbx::Toy checkerBox2dToy = playspace->MakeToy("Checkerboard");
	playspace->AddBlockTo<Tbx::Mesh>(checkerBox2dToy);
	//playspace->AddBlockTo<Tbx::Transform>(checkerBox2dToy);
	auto& material = playspace->AddBlockTo<Tbx::Material>(checkerBox2dToy);
    //material.SetTexture(0, Tbx::Texture("Assets/Checkerboard.png"));
	//material.SetColor(Tbx::Colors::Red);

	// Create camera toy
	Tbx::Toy cameraToy = playspace->MakeToy("Camera");
	playspace->AddBlockTo<Tbx::Camera>(cameraToy);
	//playspace->AddBlockTo<Tbx::Transform>(cameraToy);

	// Opens our new playspace
	playspace->Open();
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

	////const auto& shaderData = Tbx::ShaderData("viewProjection", mainWindowCam->GetViewProjectionMatrix(), Tbx::ShaderDataType::Mat4);
	////const auto& renderData = Tbx::RenderData(Tbx::RenderCommand::UploadShaderData, shaderData);
 ////   Tbx::RenderPipeline::Push(renderData);
}
