#pragma once
#include "tbx/common/uuid.h"
#include "tbx/graphics/color.h"
#include "tbx/math/matrices.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"
#include <memory>
#include <string>
#include <variant>

namespace tbx
{
    using UniformData = std::variant<bool, int, float, Vec2, Vec3, RgbaColor, Mat4>;

    // A uniform variable that can be uploaded to a shader.
    struct TBX_API ShaderUniform
    {
        std::string name = "";
        UniformData data = 0;
    };

    // The type of a shader.
    enum class TBX_API ShaderType
    {
        None,
        Vertex,
        Fragment,
        Compute
    };

    // A shader is a program that runs on the GPU and is responsible for rendering.
    // It consists of a source code and a type.
    struct TBX_API Shader
    {
        Shader() = default;
        Shader(const std::string& source, ShaderType type)
            : source(source)
            , type(type)
        {
        }

        std::string source = "";
        ShaderType type = ShaderType::None;
        Uuid id = Uuid::generate();

    };

    /// <summary>Purpose: Retrieves the shared default vertex shader instance.</summary>
    /// <remarks>Ownership: Returns a reference to the shared instance owned by the module.
    /// Thread Safety: Safe to call concurrently after static initialization.</remarks>
    TBX_API const std::shared_ptr<Shader>& get_standard_vertex_shader();

    /// <summary>Purpose: Retrieves the shared default fragment shader instance.</summary>
    /// <remarks>Ownership: Returns a reference to the shared instance owned by the module.
    /// Thread Safety: Safe to call concurrently after static initialization.</remarks>
    TBX_API const std::shared_ptr<Shader>& get_standard_fragment_shader();

    /// <summary>Provides the default vertex shader instance.</summary>
    /// <remarks>Purpose: Supplies the shared default vertex shader for new materials.
    /// Ownership: Returns a reference that participates in shared ownership of the
    /// default shader instance managed by the module.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline const std::shared_ptr<Shader>& standard_vertex_shader =
        get_standard_vertex_shader();

    /// <summary>Provides the default fragment shader instance.</summary>
    /// <remarks>Purpose: Supplies the shared default fragment shader for new materials.
    /// Ownership: Returns a reference that participates in shared ownership of the
    /// default shader instance managed by the module.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline const std::shared_ptr<Shader>& standard_fragment_shader =
        get_standard_fragment_shader();

    /// <summary>Purpose: Provides the default vertex shader instance.</summary>
    /// <remarks>Ownership: Returns a reference that participates in shared ownership
    /// of the default shader instance managed by the module.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline const std::shared_ptr<Shader>& vert_shader =
        get_standard_vertex_shader();

    /// <summary>Purpose: Provides the default fragment shader instance.</summary>
    /// <remarks>Ownership: Returns a reference that participates in shared ownership
    /// of the default shader instance managed by the module.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline const std::shared_ptr<Shader>& frag_shader =
        get_standard_fragment_shader();

    // Compiles a shader.
    class TBX_API IShaderCompiler
    {
      public:
        virtual ~IShaderCompiler() noexcept = default;

        // Compiles a shader.
        // Returns true on success and false on failure.
        virtual bool compile(std::shared_ptr<Shader> shader) = 0;
    };
}
