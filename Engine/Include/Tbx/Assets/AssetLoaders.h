#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Audio/Audio.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Model.h"
#include "Tbx/Graphics/Text.h"
#include "Tbx/Memory/Refs.h"
#include <filesystem>
#include <any>


namespace Tbx
{
    using Asset = std::any;

    class TBX_EXPORT IAssetLoader
    {
    public:
        virtual ~IAssetLoader() = default;
        virtual bool CanLoad(const std::filesystem::path& filepath) const = 0;
        virtual Asset Load(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT ITextureLoader : public IAssetLoader
    {
    public:
        Asset Load(const std::filesystem::path& filepath) final
        {
            return LoadTexture(filepath);
        }

    protected:
        virtual Ref<Texture> LoadTexture(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IShaderLoader : public IAssetLoader
    {
    public:
        Asset Load(const std::filesystem::path& filepath) final
        {
            return LoadShader(filepath);
        }

    protected:
        virtual Ref<Shader> LoadShader(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IModelLoader : public IAssetLoader
    {
    public:
        Asset Load(const std::filesystem::path& filepath) final
        {
            return LoadModel(filepath);
        }

    protected:
        virtual Ref<Model> LoadModel(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT ITextLoader : public IAssetLoader
    {
    public:
        Asset Load(const std::filesystem::path& filepath) final
        {
            return LoadText(filepath);
        }

    protected:
        virtual Ref<Text> LoadText(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IAudioLoader : public IAssetLoader
    {
    public:
        Asset Load(const std::filesystem::path& filepath) final
        {
            return LoadAudio(filepath);
        }

    protected:
        virtual Ref<Audio> LoadAudio(const std::filesystem::path& filepath) = 0;
    };
}
