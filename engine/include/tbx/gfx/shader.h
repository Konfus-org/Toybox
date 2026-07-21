#pragma once
#include "tbx/core/typedefs.h"
#include <string>
#include <vector>

namespace tbx::gpu
{
    /// @brief
    /// Purpose: GPU shader module — RAII: the destructor (defined by the selected gfx
    /// backend) releases the program. Obtain via compile_shader().
    class Shader final
    {
      public:
        explicit Shader(uint32 id)
            : _id(id)
        {
        }
        ~Shader();

      public:
        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

      public:
        /// @brief
        /// Purpose: Backend-native program id (used by the backend's draw path).
        uint32 get_id() const
        {
            return _id;
        }

      private:
        uint32 _id = 0;
    };

    /// @brief
    /// Purpose: The type of one reflected shader uniform.
    enum class UniformKind : uint8
    {
        FLOAT,
        INT,
        BOOL,
        VEC2,
        VEC3,
        VEC4,
        MAT4,
        TEXTURE,
        UNKNOWN
    };

    /// @brief
    /// Purpose: One uniform a shader exposes, discovered by reflect().
    struct UniformInfo
    {
        std::string name = {};
        UniformKind kind = UniformKind::UNKNOWN;
    };

    /// @brief
    /// Purpose: Everything a shader exposes — the schema materials program against, so any
    /// arbitrary shader "just works" without per-shader engine code.
    struct ShaderInfo
    {
        std::vector<UniformInfo> uniforms = {};
    };
}
