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
    ///  A material is a collection of shaders
    /// </summary>
    class Material : public UsesUid
    {
    public:
        /// <summary>
        /// Makes a material with the default shaders
        /// </summary>
        EXPORT Material() = default;
        EXPORT Material(const std::initializer_list<Shader>& shaders)
            : _shaders(shaders) {}
        EXPORT explicit(false) Material(const std::vector<Shader>& shaders)
            : _shaders(shaders) {}

        EXPORT const std::vector<Shader>& GetShaders() const { return _shaders; }
        EXPORT void SetShaders(const std::vector<Shader>& shaders) { _shaders = shaders; }

    private:
        std::vector<Shader> _shaders = { Shaders::DefaultFragmentShader, Shaders::DefaultVertexShader }; // default to default fragment and vertex shaders
    };

    /// <summary>
    /// A material instance is a material at runtime, it represents a material with different params and textures.
    /// </summary>
    class MaterialInstance : public UsesUid
    {
    public:
        EXPORT MaterialInstance(const Material& material, const Texture& texture)
            : _material(material), _textures({ texture }) {}
        EXPORT MaterialInstance(const Material& material, const std::initializer_list<Texture>& textures)
            : _material(material), _textures(textures) {}
        EXPORT MaterialInstance(const Material& material, const std::vector<Texture>& textures)
            : _material(material), _textures(textures) {}

        EXPORT const Material& GetMaterial() const { return _material; }

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
        const Material& _material = {};
        std::vector<Texture> _textures = { Texture() }; // default to one small white texture
        //std::unordered_map<std::string, ShaderUniform> _uniforms = {};
    };
}