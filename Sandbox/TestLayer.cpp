#include "TestLayer.h"
#include <gl/GL.h>

float _red = 1;
float _green = 0.5f;
float _blue = 0;

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
}

void TestLayer::OnEvent(Tbx::Event& event)
{
}
