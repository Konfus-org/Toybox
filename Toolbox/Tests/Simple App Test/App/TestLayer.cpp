#include "TestLayer.h"
#include <Tbx/Core/Rendering/RenderingAPI.h>
#include <Tbx/App/Render Pipeline/RenderPipelineAPI.h>
#include <Tbx/App/Input/Input.h>
#include <Tbx/App/Input/InputCodes.h>
#include <Tbx/App/Time/DeltaTime.h>
#include <Tbx/App/Windowing/WindowManager.h>
#include <Tbx/App/App.h>
#include <chrono>

float _camMoveSpeed = 1.0f;
float _camRotateSpeed = 180.0f;

void TestLayer::OnAttach()
{
	TBX_TRACE("Test layer attached!");
}

void TestLayer::OnDetach()
{
	TBX_TRACE("Test layer detached!");
}

void TestLayer::OnUpdate()
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
	if (Tbx::Input::IsKeyDown(TBX_KEY_SPACE))
	{
		_trianglePosition = _trianglePosition + (Tbx::Vector3::Up() * _camMoveSpeed * 2 * Tbx::Time::DeltaTime::Seconds());
	}

	DrawSquareTest();

	//TBX_TRACE("Camera Position: {0}", mainWindowCam->GetPosition().ToString());

	const auto& shaderData = Tbx::ShaderData("viewProjection", mainWindowCam->GetViewProjectionMatrix(), Tbx::ShaderDataType::Mat4);
	const auto& renderData = Tbx::RenderData(Tbx::RenderCommand::UploadShaderData, shaderData);
    Tbx::RenderPipeline::Push(renderData);
}
