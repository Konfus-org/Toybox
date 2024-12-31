#include "OpenGLRenderer.h"
#include "OpenGLShader.h"

namespace OpenGLRendering
{
    void OpenGLRenderer::Initialize(const std::weak_ptr<Toybox::IWindow>& context)
    {
        auto buffer = std::make_shared<OpenGLBuffer>(context);
        _buffer = buffer;
        _context = context;

        // TODO: pass shader to renderer instead of initializing here...
        _shader = std::make_shared<OpenGLShader>();

        const auto& vertexSrc = R"(
            #version 330 core

            layout(location = 0) in vec3 currentPosition;

            out vec3 position;
            
            void main()
            {
                position = currentPosition;
                gl_Position = vec4(currentPosition, 1.0);
            }
        )";


        const auto& fragmentSrc = R"(
            #version 330 core

            layout(location = 0) out vec4 color;

            in vec3 position;
            
            void main()
            {
                color = vec4(position * 0.5 + 0.5, 1.0);
            }
        )";

        _shader->Compile(vertexSrc, fragmentSrc);

        glGenVertexArrays(1, &_vertexArray);
        glBindVertexArray(_vertexArray);

        glGenBuffers(1, &_vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);

        glGenBuffers(1, &_indexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBuffer);
    }

    void OpenGLRenderer::Shutdown()
    {
        glDeleteBuffers(1, &_vertexBuffer);
        glDeleteBuffers(1, &_indexBuffer);
        glDeleteVertexArrays(1, &_vertexArray);
    }

    void OpenGLRenderer::BeginFrame()
    {
        // Clear screen at the beginning of our frame
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Bind our shader
        _shader->Bind();
    }

    void OpenGLRenderer::EndFrame()
    {
        // Draw at the end of our frame if we have anything to draw
        if (_vertexArray) glDrawElements(GL_TRIANGLES, _indicesCount, GL_UNSIGNED_INT, nullptr);
        _buffer->Swap();
    }

    void OpenGLRenderer::ClearScreen()
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::Draw(Toybox::Color color)
    {
        glClearColor(color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, color.A / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::Draw(Toybox::Mesh& mesh, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& scale)
    {
        const auto& vertices = mesh.GetVertices();
        
        // Get vertices and push them to gpu
        auto numberOfVertices = vertices.size();
        std::vector<float> meshPoints(numberOfVertices*3);
        int positionToPlace = 0;
        for (const auto& vertex : vertices)
        {
            const auto& position = vertex.Position;
            meshPoints[positionToPlace] = position.X;
            meshPoints[positionToPlace+1] = position.Y;
            meshPoints[positionToPlace+2] = position.Z;
            positionToPlace += 3;
        }

        glBufferData(GL_ARRAY_BUFFER, sizeof(meshPoints), meshPoints.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

        // Get indices and push them to gpu
        const auto& indices = mesh.GetIndices();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(), GL_STATIC_DRAW);

        // Bind vertex array and update indices count
        _indicesCount = (Toybox::uint)indices.size();
        glBindVertexArray(_vertexArray);
    }

    ////void OpenGLRenderer::Draw(const Toybox::Texture& texture, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& size)
    ////{
    ////    // TODO: Draw texture
    ////}

    void OpenGLRenderer::Draw(const std::string& text, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& size)
    {
        // TODO: Draw text
    }

    void OpenGLRenderer::SetViewport(const Toybox::Vector2I& screenPos, const Toybox::Size& size)
    {
        glViewport(screenPos.X, screenPos.Y, size.Width, size.Height);
    }

    void OpenGLRenderer::SetVSyncEnabled(const bool& enabled)
    {
        _buffer->SetSwapInterval(enabled);
    }

    std::string OpenGLRenderer::GetRendererName() const
    {
        return "OpenGL";
    }
}
