#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Color.h"
#include <vector>
#include <unordered_map>

namespace Tbx
{
    /// <summary>
    ///  A material is a collection of textures, shaders, and shader uniforms (colors, viewmatrix, etc...).
    /// </summary>
    class Material : public UsesUid
    {
    public:
        /// <summary>
        /// Makes a material with the default shaders and a small white texture.
        /// </summary>
        EXPORT Material() = default;
        EXPORT Material(const std::initializer_list<Shader>& shaders, const std::initializer_list<Texture>& textures)
            : _shaders(shaders), _textures(textures) {}
        EXPORT Material(const std::vector<Shader>& shaders, const std::vector<Texture>& textures)
            : _shaders(shaders), _textures(textures) {}
        EXPORT explicit(false) Material(const std::vector<Shader>& shaders)
            : _shaders(shaders) {}

        EXPORT const std::vector<Shader>& GetShaders() const { return _shaders; }
        EXPORT void SetShaders(const std::vector<Shader>& shaders) { _shaders = shaders; }

        /*EXPORT const ShaderUniform& GetUniform(const std::string& name) const { return _uniforms.at(name); }
        EXPORT void SetUniform(const std::string& name, const ShaderUniform& data) { _uniforms[name] = data; }*/

        EXPORT const std::vector<Texture>& GetTextures() const { return _textures; }
        EXPORT void SetTextures(const std::vector<Texture>& textures) { _textures = textures; }

        EXPORT void SetTexture(const uint& slot, const Texture& texture)
        {
            if (slot >= _textures.size()) _textures.resize(slot + 1);
            _textures[slot] = texture;
        }

    private:
        std::vector<Texture> _textures = { Texture() }; // default to one small white texture
        std::vector<Shader> _shaders = { Shaders::DefaultFragmentShader, Shaders::DefaultVertexShader }; // default to default fragment and vertex shaders
        //std::unordered_map<std::string, ShaderUniform> _uniforms = {};
    };
}