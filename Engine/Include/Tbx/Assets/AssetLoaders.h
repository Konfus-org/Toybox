#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Audio/Audio.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Model.h"
#include "Tbx/Graphics/Text.h"
#include "Tbx/Memory/Refs.h"
#include <filesystem>
#include <typeindex>
#include <utility>


namespace Tbx
{
    class Asset
    {
    public:
        Asset()
            : _type(typeid(void))
            , _data(nullptr)
        {
        }

        Asset(std::type_index type, Ref<void> data)
            : _type(type)
            , _data(std::move(data))
        {
        }

        ~Asset();

        template <typename TAsset>
        Asset(const Ref<TAsset>& data)
            : _type(typeid(TAsset))
            , _data(std::static_pointer_cast<void>(data))
        {
        }

        template <typename TAsset>
        Ref<TAsset> GetData() const
        {
            if (_data == nullptr || _type != std::type_index(typeid(TAsset)))
            {
                return nullptr;
            }

            return std::static_pointer_cast<TAsset>(_data);
        }

        std::type_index GetType() const
        {
            return _type;
        }

        bool IsValid() const
        {
            return _data != nullptr;
        }

    private:
        std::type_index _type;
        Ref<void> _data;
    };

    class TBX_EXPORT IAssetLoader
    {
    public:
        virtual ~IAssetLoader();
        virtual bool CanLoad(std::type_index type, const std::filesystem::path& filepath) const = 0;
        virtual Asset Load(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT ITextureLoader : public IAssetLoader
    {
    public:
        bool CanLoad(std::type_index type, const std::filesystem::path& filepath) const final
        {
            return type == std::type_index(typeid(Texture)) && CanLoad(filepath);
        }

        Asset Load(const std::filesystem::path& filepath) final
        {
            return Asset(LoadTexture(filepath));
        }

    protected:
        virtual bool CanLoad(const std::filesystem::path& filepath) const = 0;
        virtual Ref<Texture> LoadTexture(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IShaderLoader : public IAssetLoader
    {
    public:
        bool CanLoad(std::type_index type, const std::filesystem::path& filepath) const final
        {
            return type == std::type_index(typeid(Shader)) && CanLoad(filepath);
        }

        Asset Load(const std::filesystem::path& filepath) final
        {
            return Asset(LoadShader(filepath));
        }

    protected:
        virtual bool CanLoad(const std::filesystem::path& filepath) const = 0;
        virtual Ref<Shader> LoadShader(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IModelLoader : public IAssetLoader
    {
    public:
        bool CanLoad(std::type_index type, const std::filesystem::path& filepath) const final
        {
            return type == std::type_index(typeid(Model)) && CanLoad(filepath);
        }

        Asset Load(const std::filesystem::path& filepath) final
        {
            return Asset(LoadModel(filepath));
        }

    protected:
        virtual bool CanLoad(const std::filesystem::path& filepath) const = 0;
        virtual Ref<Model> LoadModel(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT ITextLoader : public IAssetLoader
    {
    public:
        bool CanLoad(std::type_index type, const std::filesystem::path& filepath) const final
        {
            return type == std::type_index(typeid(Text)) && CanLoad(filepath);
        }

        Asset Load(const std::filesystem::path& filepath) final
        {
            return Asset(LoadText(filepath));
        }

    protected:
        virtual bool CanLoad(const std::filesystem::path& filepath) const = 0;
        virtual Ref<Text> LoadText(const std::filesystem::path& filepath) = 0;
    };

    class TBX_EXPORT IAudioLoader : public IAssetLoader
    {
    public:
        bool CanLoad(std::type_index type, const std::filesystem::path& filepath) const final
        {
            return type == std::type_index(typeid(Audio)) && CanLoad(filepath);
        }

        Asset Load(const std::filesystem::path& filepath) final
        {
            return Asset(LoadAudio(filepath));
        }

    protected:
        virtual bool CanLoad(const std::filesystem::path& filepath) const = 0;
        virtual Ref<Audio> LoadAudio(const std::filesystem::path& filepath) = 0;
    };
}
