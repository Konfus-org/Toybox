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
    class TBX_EXPORT AssetLoader : public IAssetLoader
    {
    public:
        virtual Ref<TData> Load(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT ITextureLoader : public AssetLoader<Texture>
    {
        Ref<Texture> Load(const std::filesystem::path& filepath) final
        {
            return LoadTexture(filepath);
        }

    protected:
        virtual Ref<Texture> LoadTexture(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IShaderLoader : public AssetLoader<Shader>
    {
        Ref<Shader> Load(const std::filesystem::path& filepath) final
        {
            return LoadShader(filepath);
        }

    protected:
        virtual Ref<Shader> LoadShader(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IModelLoader : public AssetLoader<Model>
    {
        Ref<Model> Load(const std::filesystem::path& filepath) final
        {
            return LoadModel(filepath);
        }

    protected:
        virtual Ref<Model> LoadModel(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT ITextLoader : public AssetLoader<Text>
    {
        Ref<Text> Load(const std::filesystem::path& filepath) final
        {
            return LoadText(filepath);
        }

    protected:
        virtual Ref<Text> LoadText(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IAudioLoader : public AssetLoader<Audio>
    {
        Ref<Audio> Load(const std::filesystem::path& filepath) final
        {
            return LoadAudio(filepath);
        }

    protected:
        virtual Ref<Audio> LoadAudio(const std::filesystem::path& filepath) = 0;
    };
}
