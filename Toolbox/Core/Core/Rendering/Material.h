#pragma once
#include "Core/Rendering/Shader.h"
#include "Core/Rendering/Texture.h"

namespace Tbx
{
    class Material
    {
    public:
        TBX_API Material() = default;
        TBX_API Material(const Shader& shader, const std::vector<Texture>& textures)
            : _shader(shader), _textures(textures) {}
        TBX_API explicit(false) Material(const Shader& shader)
            : _shader(shader) {}
        TBX_API ~Material() = default;

        TBX_API const Shader& GetShader() const { return _shader; }
        TBX_API const std::vector<Texture>& GetTextures() const { return _textures; }

    private:
        Shader _shader;
        std::vector<Texture> _textures;
    };
}