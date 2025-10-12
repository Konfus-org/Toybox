#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Audio/Audio.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Model.h"
#include "Tbx/Graphics/Text.h"
#include <filesystem>

namespace Tbx
{
    class TBX_EXPORT IAssetLoader
    {
    public:
        virtual ~IAssetLoader() = default;
        virtual bool CanLoad(const std::filesystem::path& filepath) const = 0;
    };

    template<typename TData>
    class TBX_EXPORT AssetLoader : virtual public IAssetLoader
    {
    public:
        virtual TData Load(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT ITextureLoader : public AssetLoader<Texture>
    {
        Texture Load(const std::filesystem::path& filepath) final
        {
            return LoadTexture(filepath);
        }

    protected:
        virtual Texture LoadTexture(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IShaderLoader : public AssetLoader<Shader>
    {
        Shader Load(const std::filesystem::path& filepath) final
        {
            return LoadShader(filepath);
        }

    protected:
        virtual Shader LoadShader(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IModelLoader : public AssetLoader<Model>
    {
        Model Load(const std::filesystem::path& filepath) final
        {
            return LoadModel(filepath);
        }

    protected:
        virtual Model LoadModel(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT ITextLoader : public AssetLoader<Text>
    {
        Text Load(const std::filesystem::path& filepath) final
        {
            return LoadText(filepath);
        }

    protected:
        virtual Text LoadText(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IAudioLoader : public AssetLoader<Audio>
    {
        Audio Load(const std::filesystem::path& filepath) final
        {
            return LoadAudio(filepath);
        }

    protected:
        virtual Audio LoadAudio(const std::filesystem::path& filepath) = 0;
    };
}
