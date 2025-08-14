#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Color.h"
#include <vector>
#include <unordered_map>

namespace Tbx
{
    /// <summary>
    ///  A material is a shader, shader data (colors, viewmatrix, etc...), and textures.
    /// </summary>
    class Material : public UsesUid
    {
    public:
        /// <summary>
        /// Makes a material with the default shader and no textures.
        /// </summary>
        EXPORT Material() = default;
        EXPORT Material(const Shader& vertShader, const Shader& fragShader, const std::vector<Texture>& textures) 
            : _vertexShader(vertShader), _fragmentShader(fragShader), _textures(textures) {}
        EXPORT explicit(false) Material(const Shader& vertShader, const Shader& fragShader)
            : _vertexShader(vertShader), _fragmentShader(fragShader) {}

        EXPORT const Shader& GetVertexShader() const { return _vertexShader; }
        EXPORT void SetVertexShader(const Shader& shader) { _vertexShader = shader; }

        EXPORT const Shader& GetFragmentShader() const { return _fragmentShader; }
        EXPORT void SetFragmentShader(const Shader& shader) { _fragmentShader = shader; }

        EXPORT const ShaderData& GetData(const std::string& name) const { return _data.at(name); }
        EXPORT void SetData(const std::string& name, const ShaderData& data) { _data[name] = data; }

        EXPORT const std::vector<Texture>& GetTextures() const { return _textures; }
        EXPORT void SetTextures(const std::vector<Texture>& textures) { _textures = textures; }

        EXPORT void SetTexture(const uint& slot, const Texture& texture)
        {
            if (slot >= _textures.size()) _textures.resize(slot + 1);
            _textures[slot] = texture;
        }

    private:
        Shader _vertexShader = Shaders::DefaultVertexShader;
        Shader _fragmentShader = Shaders::DefaultFragmentShader;
        std::unordered_map<std::string, ShaderData> _data = {};
        std::vector<Texture> _textures = { Texture() }; // default to one small white texture
    };
}