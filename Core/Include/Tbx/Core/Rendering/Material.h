#pragma once
#include "Tbx/Core/Rendering/Shader.h"
#include "Tbx/Core/Rendering/Texture.h"
#include "Tbx/Core/Rendering/Color.h"
#include <vector>

namespace Tbx
{
    class Material
    {
    public:
        /// <summary>
        /// Makes a material with the default shader and no textures.
        /// </summary>
        EXPORT Material() = default;
        EXPORT Material(const Shader& shader, const std::vector<Texture>& textures) 
            : _shader(shader), _textures(textures) {}
        EXPORT explicit(false) Material(const Shader& shader) 
            : _shader(shader) {}

        EXPORT const Shader& GetShader() const { return _shader; }
        EXPORT const std::vector<Texture>& GetTextures() const { return _textures; }
        EXPORT const Color& GetColor() const { return _color; }

        EXPORT void SetShader(const Shader& shader) { _shader = shader; }
        EXPORT void SetColor(const Color& color) { _color = color; }
        EXPORT void SetTextures(const std::vector<Texture>& textures) { _textures = textures; }

        EXPORT void SetTexture(const uint& slot, const Texture& texture)
        {
            if (slot >= _textures.size()) _textures.resize(slot + 1);
            _textures[slot] = texture;
        }

    private:
        Shader _shader = {};
        Color _color = Colors::White;
        std::vector<Texture> _textures = {};
    };
}