#include "TestLayer.h"
#include <gl/GL.h>


std::chrono::high_resolution_clock::time_point _lastTime;
int _frameCount = 0;
float _fps = 0;

float _red = 1;
float _green = 0.5f;
float _blue = 0;

static void CalucateAndPrintFPS()
{
	_frameCount++;
	auto currentTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float> elapsed = currentTime - _lastTime;

	// If one second has passed, calculate FPS
	if (elapsed.count() >= 1.0f) 
	{
		_fps = _frameCount / elapsed.count();
		_frameCount = 0;
		_lastTime = currentTime;
	}

	TBX_INFO("FPS: {0}", _fps);
}

static void SetShaderTest()
{
	// Pass shader to renderer
	const auto& vertexSrc = R"(
            #version 330 core

            layout(location = 0) in vec3 inPosition;
            layout(location = 1) in vec4 inColor;

            out vec3 position;
            out vec4 color;
            
            void main()
            {
                position = inPosition;
                color = inColor;
                gl_Position = vec4(inPosition, 1.0);
            }
        )";

	const auto& fragmentSrc = R"(
            #version 330 core

            layout(location = 0) out vec4 outColor;

            in vec3 position;
            in vec4 color;
            
            void main()
            {
                outColor = vec4(position * 0.5 + 0.5, 1.0);
                outColor = color;
            }
        )";

	const auto& shader = Tbx::Shader(vertexSrc, fragmentSrc);
	Tbx::Rendering::Submit(Tbx::RenderCommand::SetShader, shader);
}

static void ChangeWindowColorTest()
{
	if (_red > 1)
		_red = 0;
	if (_red < 0)
		_red = 1;

	if (_green > 1)
		_green = 0;
	if (_green < 0)
		_green = 1;

	if (_blue > 1)
		_blue = 0;
	if (_blue < 0)
		_blue = 1;

	_red += 0.001f;
	_green += 0.001f;
	_blue += 0.001f;

	Tbx::Rendering::Submit(Tbx::RenderCommand::RenderColor, Tbx::Color(_red, _green, _blue, 1.0f));
}

static void DrawSquareTest()
{
	const auto& sqaureMeshVerts =
	{
		Tbx::Vertex(Tbx::Vector3(-0.5f, -0.5f, 0.0f), Tbx::Color(0.0f, 0.8f, 0.1f, 1.0f)),
		Tbx::Vertex(Tbx::Vector3(0.5f, -0.5f, 0.0f), Tbx::Color(0.0f, 0.8f, 0.1f, 1.0f)),
		Tbx::Vertex(Tbx::Vector3(0.5f, 0.5f, 0.0f), Tbx::Color(0.0f, 0.8f, 0.1f, 1.0f)),
		Tbx::Vertex(Tbx::Vector3(-0.5f, 0.5f, 0.0f), Tbx::Color(0.0f, 0.8f, 0.1f, 1.0f))
	};
	const std::vector<Tbx::uint32>& squareMeshIndices = { 0, 1, 2, 2, 3, 0 };
	const auto& squareMesh = Tbx::Mesh(sqaureMeshVerts, squareMeshIndices);

	Tbx::Rendering::Submit(Tbx::RenderCommand::RenderMesh, squareMesh);
}

static void DrawTriangleTest()
{
	const auto& triangleMeshVerts =
	{
		Tbx::Vertex(Tbx::Vector3(-0.5f, -0.5f, 0.0f), Tbx::Color(0.8f, 0.2f, 0.1f, 1.0f)),
		Tbx::Vertex(Tbx::Vector3(0.5f, -0.5f, 0.0f), Tbx::Color(0.1f, 0.8f, 0.2f, 1.0f)),
		Tbx::Vertex(Tbx::Vector3(0.0f, 0.5f, 0.0f), Tbx::Color(0.2f, 0.1f, 0.8f, 1.0f)),
	};
	const std::vector<Tbx::uint32>& triangleMeshIndices = { 0, 1, 2 };
	const auto& triangleMesh = Tbx::Mesh(triangleMeshVerts, triangleMeshIndices);

	Tbx::Rendering::Submit(Tbx::RenderCommand::RenderMesh, triangleMesh);
}

void TestLayer::OnAttach()
{
	TBX_TRACE("Main layer attached!");
	SetShaderTest();
}

void TestLayer::OnDetach()
{
	TBX_TRACE("Main layer detached!");
}

void TestLayer::OnUpdate()
{
	if (Tbx::Input::IsKeyDown(TBX_KEY_SPACE)) TBX_TRACE("Space pressed!");
	
	ChangeWindowColorTest();
    DrawSquareTest();
    DrawTriangleTest();

    CalucateAndPrintFPS();
}

void TestLayer::OnEvent(Tbx::Event& event)
{
}
