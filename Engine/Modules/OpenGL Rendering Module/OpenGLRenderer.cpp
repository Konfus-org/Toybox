#include "OpenGLRenderer.h"

namespace OpenGLRendering
{
    void OpenGLRenderer::Initialize(const std::weak_ptr<Toybox::IWindow>& context)
    {
        auto buffer = std::make_shared<OpenGLBuffer>(context);
        _buffer = buffer;
        _context = context;

        // TODO: move this out to a seperate shader class
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Basic OpenGL setup
        glGenVertexArrays(1, &_vao);
        glBindVertexArray(_vao);

        // Simple shader for rendering
        const std::string vertexSrc = R"(
            #version 330 core

            layout(location = 0) in vec3 aPosition;  // Vertex position
            layout(location = 1) in vec3 aColor;     // Vertex color
            layout(location = 2) in vec2 aTexCoord;  // Texture coordinates

            uniform mat4 model;  // Transformation matrix
            uniform mat4 view;   // View matrix (camera)
            uniform mat4 projection; // Projection matrix (perspective)

            out vec3 fragColor;     // Pass color to the fragment shader
            out vec2 fragTexCoord;  // Pass texture coordinates to the fragment shader

            void main()
            {
                // Apply the transformation matrix to the vertex position
                gl_Position = projection * view * model * vec4(aPosition, 1.0f);

                // Pass the color and texture coordinates to the fragment shader
                fragColor = aColor;
                fragTexCoord = aTexCoord;
            }
        )";

        const std::string fragmentSrc = R"(
            #version 330 core

            in vec3 fragColor;      // Color passed from vertex shader
            in vec2 fragTexCoord;   // Texture coordinates passed from vertex shader

            out vec4 FragColor;     // Output color

            uniform sampler2D textureSampler; // Texture sampler (for textures)

            void main()
            {
                // Simple color output (could be mixed with texture if needed)
                vec4 textureColor = texture(textureSampler, fragTexCoord);
                FragColor = textureColor * vec4(fragColor, 1.0f);  // Combine texture and color
            }
        )";

        _shaderProgram = LoadShader(vertexSrc, fragmentSrc);
        TBX_ASSERT(_shaderProgram, "Failed to load shader program!");
    }

    void OpenGLRenderer::Shutdown()
    {
        glDeleteProgram(_shaderProgram);
        glDeleteBuffers(1, &_vbo);
        glDeleteBuffers(1, &_ibo);
        glDeleteVertexArrays(1, &_vao);
    }

    void OpenGLRenderer::BeginFrame()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::EndFrame()
    {
        _buffer->Swap();
    }

    void OpenGLRenderer::ClearScreen()
    {
        glUseProgram(_shaderProgram);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::Draw(Toybox::Color color)
    {
        glUseProgram(_shaderProgram);
        glClearColor(color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, color.A / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::Draw(Toybox::Mesh& mesh, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& scale)
    {
        const auto& posMatrix = worldPos.ToMatrix();
        const auto& scaleMatrix = scale.ToMatrix();
        const auto& rotationMatrix = rotation.ToMatrix();

        const auto& posAndRotMatrix = posMatrix * rotationMatrix;
        auto modelMatrix = posAndRotMatrix * scaleMatrix;

        GLfloat glModelMatrix[16];
        for (int i = 0; i < 16; i++)
        {
            glModelMatrix[i] = modelMatrix.Data[i];
        }

        GLuint modelLoc = glGetUniformLocation(_shaderProgram, "model");
        GLuint viewLoc = glGetUniformLocation(_shaderProgram, "view");
        GLuint projLoc = glGetUniformLocation(_shaderProgram, "projection");

        glUseProgram(_shaderProgram);

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glModelMatrix);

        // Set up view and projection matrices

        // View matrix (camera at (0, 0, 3), looking at origin)
        GLfloat viewMatrix[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, -3.0f,  // Camera at Z = 3
            0.0f, 0.0f, 0.0f, 1.0f
        };

        // Projection matrix (simple perspective projection)
        GLfloat projMatrix[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f, -2.0f,
            0.0f, 0.0f, -0.1f, 1.0f
        };

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, viewMatrix);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, projMatrix);

        GLuint VAO, VBO;
        glGenVertexArrays(1, &VAO);   // Generate VAO
        glGenBuffers(1, &VBO);        // Generate VBO

        glBindVertexArray(VAO);       // Bind VAO

        // Bind the VBO and load vertex data
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, mesh.GetVertices().size() * sizeof(float), mesh.GetVertices().data(), GL_STATIC_DRAW);

        // Set vertex attribute pointers
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); // Positions
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); // Colors
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); // Texture coords
        glEnableVertexAttribArray(2);

        // Draw the mesh
        glDrawArrays(GL_TRIANGLES, 0, mesh.GetVertices().size());

        glBindVertexArray(0);
    }

    void OpenGLRenderer::Draw(const Toybox::Texture& texture, const Toybox::Vector3& worldPos, const Toybox::Quaternion& rotation, const Toybox::Scale& size)
    {
        // TODO: Draw texture
    }

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

    GLuint OpenGLRenderer::LoadShader(const std::string& vertexSrc, const std::string& fragmentSrc) const
    {
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        const char* vertexCode = vertexSrc.c_str();
        glShaderSource(vertexShader, 1, &vertexCode, nullptr);
        glCompileShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        const char* fragmentCode = fragmentSrc.c_str();
        glShaderSource(fragmentShader, 1, &fragmentCode, nullptr);
        glCompileShader(fragmentShader);

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return program;
    }
}
