#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Model.h"
#include "Tbx/Graphics/Text.h"
#include <filesystem>

namespace Tbx
{
    /// <summary>
    /// Base interface implemented by all asset loaders regardless of the asset type.
    /// </summary>
    class EXPORT IAssetLoader
    {
    public:
        virtual ~IAssetLoader() = default;

        /// <summary>
        /// Determines whether the loader can handle the asset located at <paramref name="filepath"/>.
        /// </summary>
        virtual bool CanLoad(const std::filesystem::path& filepath) const = 0;
    };

    /// <summary>
    /// Generic typed loader that produces instances of <typeparamref name="TData"/>.
    /// </summary>
    template<typename TData>
    class EXPORT AssetLoader : public IAssetLoader
    {
    public:
        /// <summary>
        /// Loads the requested asset and returns the populated object.
        /// </summary>
        virtual TData Load(const std::filesystem::path& filepath) = 0;
    };

    /// <summary>
    /// Specialized loader interface for textures.
    /// </summary>
    class EXPORT ITextureLoader : public AssetLoader<Texture>
    {
        Texture Load(const std::filesystem::path& filepath) final
        {
            return LoadTexture(filepath);
        }

    protected:
        /// <summary>Performs the actual texture loading implementation.</summary>
        virtual Texture LoadTexture(const std::filesystem::path& filepath) = 0;
    };

    /// <summary>
    /// Specialized loader interface for shaders.
    /// </summary>
    class EXPORT IShaderLoader : public AssetLoader<Shader>
    {
        Shader Load(const std::filesystem::path& filepath) final
        {
            return LoadShader(filepath);
        }

    protected:
        /// <summary>Performs the actual shader loading implementation.</summary>
        virtual Shader LoadShader(const std::filesystem::path& filepath) = 0;
    };

    /// <summary>
    /// Specialized loader interface for 3D models.
    /// </summary>
    class EXPORT IModelLoader : public AssetLoader<Model>
    {
        Model Load(const std::filesystem::path& filepath) final
        {
            return LoadModel(filepath);
        }

    protected:
        /// <summary>Performs the actual model loading implementation.</summary>
        virtual Model LoadModel(const std::filesystem::path& filepath) = 0;
    };

    /// <summary>
    /// Specialized loader interface for text-based assets.
    /// </summary>
    class EXPORT ITextLoader : public AssetLoader<Text>
    {
        Text Load(const std::filesystem::path& filepath) final
        {
            return LoadText(filepath);
        }

    protected:
        /// <summary>Performs the actual text loading implementation.</summary>
        virtual Text LoadText(const std::filesystem::path& filepath) = 0;
    };
}

