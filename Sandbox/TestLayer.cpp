#include "TestLayer.h"
#include <gl/GL.h>

int _frameCount;
float _red = 1;
float _green = 0.5f;
float _blue = 0;

static void ChangeWindowColorTest()
{
	glClearColor(_red, _green, _blue, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

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
	//ChangeWindowColorTest();

	// Draw square
	const auto& sqaureMeshVerts =
	{
		Tbx::Vertex(Tbx::Vector3(-0.5f, -0.5f, 0.0f), Tbx::Color(0.0f, 0.8f, 0.1f, 1.0f)),
		Tbx::Vertex(Tbx::Vector3(0.5f, -0.5f, 0.0f), Tbx::Color(0.0f, 0.8f, 0.1f, 1.0f)),
		Tbx::Vertex(Tbx::Vector3(0.5f, 0.5f, 0.0f), Tbx::Color(0.0f, 0.8f, 0.1f, 1.0f)),
		Tbx::Vertex(Tbx::Vector3(-0.5f, 0.5f, 0.0f), Tbx::Color(0.0f, 0.8f, 0.1f, 1.0f))
	};
	const std::vector<Tbx::uint32>& squareMeshIndices = { 0, 1, 2, 2, 3, 0 };
	const auto& squareMesh = Tbx::Mesh(sqaureMeshVerts, squareMeshIndices);
	const auto& drawSquareCmd = Tbx::DrawMeshCommand(squareMesh);

	// Draw triangle
	const auto& triangleMeshVerts =
	{
		Tbx::Vertex(Tbx::Vector3(-0.5f, -0.5f, 0.0f), Tbx::Color(0.8f, 0.2f, 0.1f, 1.0f)),
		Tbx::Vertex(Tbx::Vector3(0.5f, -0.5f, 0.0f), Tbx::Color(0.1f, 0.8f, 0.2f, 1.0f)),
		Tbx::Vertex(Tbx::Vector3(0.0f, 0.5f, 0.0f), Tbx::Color(0.2f, 0.1f, 0.8f, 1.0f)),
	};
	const std::vector<Tbx::uint32>& triangleMeshIndices = { 0, 1, 2 };
	const auto& triangleMesh = Tbx::Mesh(triangleMeshVerts, triangleMeshIndices);
	const auto& drawTriangleCmd = Tbx::DrawMeshCommand(triangleMesh);

	////Tbx::RenderQueue::Enqueue(drawSquareCmd);
	////Tbx::RenderQueue::Enqueue(drawTriangleCmd);
}

void TestLayer::OnEvent(Tbx::Event& event)
{
}
