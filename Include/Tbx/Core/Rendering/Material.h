#pragma once
#include "Tbx/Core/Rendering/Shader.h"
#include "Tbx/Core/Rendering/Texture.h"

namespace Tbx
{
    class Material
    {
    public:
        EXPORT Material() = default;
        EXPORT Material(const Shader& shader, const std::vector<Texture>& textures)
            : _shader(shader), _textures(textures) {}
        EXPORT explicit(false) Material(const Shader& shader)
            : _shader(shader) {}
        EXPORT ~Material() = default;

        EXPORT const Shader& GetShader() const { return _shader; }
        EXPORT const std::vector<Texture>& GetTextures() const { return _textures; }

    private:
        Shader _shader;
        std::vector<Texture> _textures;
    };
}