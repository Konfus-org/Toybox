#include "TestLayer.h"
#include <Tbx/Core/Rendering/RenderingAPI.h>
#include <Tbx/App/Render Pipeline/RenderPipelineAPI.h>
#include <Tbx/App/Input/Input.h>
#include <Tbx/App/Input/InputCodes.h>
#include <Tbx/App/Time/DeltaTime.h>
#include <Tbx/App/App.h>
#include <chrono>

float _camMoveSpeed = 1.0f;
float _camRotateSpeed = 180.0f;

Tbx::Material _testMat;
Tbx::Vector3 _trianglePosition = Tbx::Vector3::Zero();

static void CreateTestMat()
{
	// Upload shader to renderer
	const auto& vertexSrc = R"(
            #version 330 core

            layout(location = 0) in vec3 inPosition;
            layout(location = 1) in vec4 inColor;
            layout(location = 2) in vec4 inNormal; // TODO: implement normals!
            layout(location = 3) in vec2 inTextureCoord;

            uniform mat4 viewProjection;
            uniform mat4 transform;

            out vec4 color;
            out vec4 normal;
            out vec2 textureCoord;
            
            void main()
            {
                color = inColor;
                textureCoord = inTextureCoord;
                gl_Position = viewProjection * transform * vec4(inPosition, 1.0);
            }
        )";

	const auto& fragmentSrc = R"(
            #version 330 core

            layout(location = 0) out vec4 outColor;

            in vec4 color;
            in vec4 normal; // TODO: implement normals!
            in vec2 textureCoord;

            uniform sampler2D textureUniform;
            
            void main()
            {
                outColor = texture(textureUniform, textureCoord);
            }
        )";

	const auto& shader = Tbx::Shader(vertexSrc, fragmentSrc);
	Tbx::RenderPipeline::Push(Tbx::RenderCommand::UploadShader, shader);

	// Upload texture to renderer
	////auto testTex = Tbx::Texture("Assets/Checkerboard.png");
	////Tbx::RenderPipeline::Push(Tbx::RenderCommand::UploadTexture, Tbx::TextureRenderData(testTex, 0));
	// Uploading texture position (0 rn for diffuse, add more for other textures like normals, height, etc...)
	// TODO: automate the texture position
	Tbx::RenderPipeline::Push(Tbx::RenderCommand::UploadShaderData, Tbx::ShaderData("textureUniform", 0, Tbx::ShaderDataType::Int));

	// Create test material
	//_testMat = Tbx::Material(shader, { testTex });
}

static void DrawSquareTest()
{
	const auto& sqaureMeshVerts =
	{
		Tbx::Vertex(
			Tbx::Vector3(-0.5f, -0.5f, 0.0f),    // Position
			Tbx::Vector3(0.0f, 0.0f, 0.0f),      // Normal
			Tbx::Vector2I(0.0f, 0.0f),           // Texture coordinates
			Tbx::Color(0.0f, 0.0f, 0.0f, 1.0f)), // Color

		Tbx::Vertex(
			Tbx::Vector3(0.5f, -0.5f, 0.0f),     // Position
			Tbx::Vector3(0.0f, 0.0f, 0.0f),      // Normal
			Tbx::Vector2I(1.0f, 0.0f),           // Texture coordinates
			Tbx::Color(0.0f, 0.0f, 0.0f, 1.0f)), // Color

		Tbx::Vertex(
			Tbx::Vector3(0.5f, 0.5f, 0.0f),		 // Position
			Tbx::Vector3(0.0f, 0.0f, 0.0f),      // Normal
			Tbx::Vector2I(1.0f, 1.0f),           // Texture coordinates
			Tbx::Color(0.0f, 0.0f, 0.0f, 1.0f)), // Color

		Tbx::Vertex(
			Tbx::Vector3(-0.5f, 0.5f, 0.0f),     // Position
			Tbx::Vector3(0.0f, 0.0f, 0.0f),      // Normal
			Tbx::Vector2I(0.0f, 1.0f),           // Texture coordinates
			Tbx::Color(0.0f, 0.0f, 0.0f, 1.0f))  // Color
	};
	const std::vector<Tbx::uint32>& squareMeshIndices = { 0, 1, 2, 2, 3, 0 };
	const auto& squareMesh = Tbx::Mesh(sqaureMeshVerts, squareMeshIndices);

	Tbx::RenderPipeline::Push(Tbx::RenderCommand::UploadShaderData, Tbx::ShaderData("transform", Tbx::Mat4x4::FromPosition(_trianglePosition), Tbx::ShaderDataType::Mat4));
	Tbx::RenderPipeline::Push(Tbx::RenderCommand::RenderMesh, Tbx::MeshRenderData(squareMesh, _testMat));
}

void TestLayer::OnAttach()
{
	TBX_TRACE("Test layer attached!");

	CreateTestMat();

	// Configure camera
	const auto& mainWindow = Tbx::App::GetInstance().lock()->GetMainWindow();
	const auto& mainWindowCam = mainWindow.lock()->GetCamera().lock();
	const auto& mainWindowSize = mainWindow.lock()->GetSize();

	// Test perspective camera
	mainWindowCam->SetPerspective(45.0f, mainWindowSize.AspectRatio(), 0.1f, 100);
	mainWindowCam->SetPosition(Tbx::Vector3(0.0f, 0.0f, -5.0f));
	mainWindowCam->SetRotation(Tbx::Quaternion::FromEuler(Tbx::Vector3(0.0f, 0.0f, 0.0f)));

	// Test ortho camera
	////mainWindowCam->SetOrthagraphic(1, mainWindowSize.AspectRatio(), -1, 10);
	////mainWindowCam->SetPosition(Tbx::Vector3(0.0f, 0.0f, -1.0f));

	Tbx::RenderPipeline::SetVSyncEnabled(true);
	Tbx::RenderPipeline::Push(Tbx::RenderCommand::UploadShaderData, Tbx::ShaderData("viewProjection", mainWindowCam->GetViewProjectionMatrix(), Tbx::ShaderDataType::Mat4));
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

	const auto& mainWindow = Tbx::App::GetInstance().lock()->GetMainWindow();
	const auto& mainWindowCam = mainWindow.lock()->GetCamera().lock();

	// Camera movement and rotation
	////if (Tbx::Input::IsKeyDown(TBX_KEY_W))
	////{
	////	mainWindowCam->SetPosition(mainWindowCam->GetPosition() + (Tbx::Vector3::Up() * _camMoveSpeed * Tbx::Time::DeltaTime::Seconds()));
	////}
 ////   else if (Tbx::Input::IsKeyDown(TBX_KEY_S))
 ////   {
 ////       mainWindowCam->SetPosition(mainWindowCam->GetPosition() + (Tbx::Vector3::Down() * _camMoveSpeed * Tbx::Time::DeltaTime::Seconds()));
 ////   }

 ////   if (Tbx::Input::IsKeyDown(TBX_KEY_A))
 ////   {
 ////       mainWindowCam->SetPosition(mainWindowCam->GetPosition() + (Tbx::Vector3::Left() * _camMoveSpeed) * Tbx::Time::DeltaTime::Seconds());
 ////   }
 ////   else if (Tbx::Input::IsKeyDown(TBX_KEY_D))
 ////   {
 ////       mainWindowCam->SetPosition(mainWindowCam->GetPosition() + (Tbx::Vector3::Right() * _camMoveSpeed * Tbx::Time::DeltaTime::Seconds()));
 ////   }

 ////   if (Tbx::Input::IsKeyDown(TBX_KEY_UP))
 ////   {
 ////       mainWindowCam->SetPosition(mainWindowCam->GetPosition() + (Tbx::Vector3::Forward() * _camMoveSpeed * Tbx::Time::DeltaTime::Seconds()));
 ////   }
 ////   else if (Tbx::Input::IsKeyDown(TBX_KEY_DOWN))
 ////   {
 ////       mainWindowCam->SetPosition(mainWindowCam->GetPosition() + (Tbx::Vector3::Backward() * _camMoveSpeed * Tbx::Time::DeltaTime::Seconds()));
 ////   }

 ////   if (Tbx::Input::IsKeyDown(TBX_KEY_Q))
 ////   {
	////	const auto& currentRot = mainWindowCam->GetRotation();
	////	const auto& newRot = currentRot * (Tbx::Vector3(0.0f, 0.0f, -1.0f) * _camRotateSpeed * Tbx::Time::DeltaTime::Seconds());
	////	mainWindowCam->SetRotation(newRot);
 ////   }
 ////   else if (Tbx::Input::IsKeyDown(TBX_KEY_E))
 ////   {
	////	const auto& currentRot = mainWindowCam->GetRotation();
	////	const auto& newRot = currentRot * (Tbx::Vector3(0.0f, 0.0f, 1.0f) * _camRotateSpeed * Tbx::Time::DeltaTime::Seconds());
	////	mainWindowCam->SetRotation(newRot);
 ////   }

	////// Triangle movement
	////if (Tbx::Input::IsKeyDown(TBX_KEY_SPACE))
	////{
	////	_trianglePosition = _trianglePosition + (Tbx::Vector3::Up() * _camMoveSpeed * 2 * Tbx::Time::DeltaTime::Seconds());
	////}

	DrawSquareTest();

	//TBX_TRACE("Camera Position: {0}", mainWindowCam->GetPosition().ToString());

    Tbx::RenderPipeline::Push(Tbx::RenderCommand::UploadShaderData, Tbx::ShaderData("viewProjection", mainWindowCam->GetViewProjectionMatrix(), Tbx::ShaderDataType::Mat4));
}
