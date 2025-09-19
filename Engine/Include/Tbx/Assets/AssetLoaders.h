#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Model.h"
#include "Tbx/Graphics/Text.h"
#include <filesystem>

namespace Tbx
{
    class EXPORT IAssetLoader
    {
    public:
        virtual bool CanLoad(const std::filesystem::path& filepath) const = 0;
    };

    template<typename TData>
    class EXPORT AssetLoader : public IAssetLoader
    {
    public:
        virtual TData Load(const std::filesystem::path& filepath) = 0;
    };

    class EXPORT ITextureLoader : AssetLoader<Texture>
    {
        Texture Load(const std::filesystem::path& filepath) final
        {
            return LoadTexture(filepath);
        }

    protected:
        virtual Texture LoadTexture(const std::filesystem::path& filepath) = 0;
    };

    class EXPORT IShaderLoader : AssetLoader<Shader>
    {
        Shader Load(const std::filesystem::path& filepath) final
        {
            return LoadShader(filepath);
        }

    protected:
        virtual Shader LoadShader(const std::filesystem::path& filepath) = 0;
    };

    class EXPORT IModelLoader : AssetLoader<Model>
    {
        Model Load(const std::filesystem::path& filepath) final
        {
            return LoadModel(filepath);
        }

    protected:
        virtual Model LoadModel(const std::filesystem::path& filepath) = 0;
    };

    class EXPORT ITextLoader : AssetLoader<Text>
    {
        Text Load(const std::filesystem::path& filepath) final
        {
            return LoadText(filepath);
        }

    protected:
        virtual Text LoadText(const std::filesystem::path& filepath) = 0;
    };
}