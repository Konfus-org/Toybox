#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Audio/Audio.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Model.h"
#include "Tbx/Graphics/Text.h"
#include "Tbx/Ids/Guid.h"
#include "Tbx/Memory/Refs.h"
#include <filesystem>
#include <string>

namespace Tbx
{
    struct TBX_EXPORT Asset
    {
        virtual ~Asset() = default;

        Guid Id = Guid::Invalid;
        std::string Name = "";
        std::filesystem::path FilePath = {};
    };

    struct TBX_EXPORT TextureAsset : public Asset
    {
        Ref<Texture> Data = nullptr;
    };

    struct TBX_EXPORT ShaderAsset : public Asset
    {
        Ref<Shader> Data = nullptr;
    };

    struct TBX_EXPORT ModelAsset : public Asset
    {
        Ref<Model> Data = nullptr;
    };

    struct TBX_EXPORT TextAsset : public Asset
    {
        Ref<Text> Data = nullptr;
    };

    struct TBX_EXPORT AudioAsset : public Asset
    {
        Ref<Audio> Data = nullptr;
    };

    class TBX_EXPORT IAssetLoader
    {
    public:
        virtual ~IAssetLoader() = default;
        virtual bool CanLoad(const std::filesystem::path& filepath) const = 0;
        virtual Ref<Asset> Load(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT ITextureLoader : public IAssetLoader
    {
        Ref<Asset> Load(const std::filesystem::path& filepath) final
        {
            return LoadTexture(filepath);
        }

    protected:
        virtual Ref<TextureAsset> LoadTexture(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IShaderLoader : public IAssetLoader
    {
        Ref<Asset> Load(const std::filesystem::path& filepath) final
        {
            return LoadShader(filepath);
        }

    protected:
        virtual Ref<ShaderAsset> LoadShader(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IModelLoader : public IAssetLoader
    {
        Ref<Asset> Load(const std::filesystem::path& filepath) final
        {
            return LoadModel(filepath);
        }

    protected:
        virtual Ref<ModelAsset> LoadModel(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT ITextLoader : public IAssetLoader
    {
        Ref<Asset> Load(const std::filesystem::path& filepath) final
        {
            return LoadText(filepath);
        }

    protected:
        virtual Ref<TextAsset> LoadText(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IAudioLoader : public IAssetLoader
    {
        Ref<Asset> Load(const std::filesystem::path& filepath) final
        {
            return LoadAudio(filepath);
        }

    protected:
        virtual Ref<AudioAsset> LoadAudio(const std::filesystem::path& filepath) = 0;
    };
}
