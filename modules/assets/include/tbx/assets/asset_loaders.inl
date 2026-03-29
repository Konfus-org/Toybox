#pragma once

namespace tbx
{
    template <typename TAsset>
    struct AssetPromise
    {
        std::shared_ptr<TAsset> asset;
        std::shared_future<Result> promise;
    };

    template <typename TAsset>
    struct AssetLoader
    {
        static AssetPromise<TAsset> load_async(const std::filesystem::path& asset_path)
        {
            static_assert(sizeof(TAsset) == 0, "No async asset loader registered for this type.");
            return load_async(asset_path);
        }

        static std::shared_ptr<TAsset> load(const std::filesystem::path& asset_path)
        {
            static_assert(sizeof(TAsset) == 0, "No asset loader registered for this type.");
            return load(asset_path);
        }
    };

    template <>
    struct AssetLoader<Model>
    {
        static AssetPromise<Model> load_async(const std::filesystem::path& asset_path)
        {
            return load_model_async(asset_path);
        }

        static std::shared_ptr<Model> load(const std::filesystem::path& asset_path)
        {
            return load_model(asset_path);
        }
    };

    template <>
    struct AssetLoader<Texture>
    {
        static AssetPromise<Texture> load_async(const std::filesystem::path& asset_path)
        {
            return load_texture_async(
                asset_path,
                TextureWrap::REPEAT,
                TextureFilter::LINEAR,
                TextureFormat::RGBA,
                TextureMipmaps::ENABLED,
                TextureCompression::AUTO);
        }

        static std::shared_ptr<Texture> load(const std::filesystem::path& asset_path)
        {
            return load_texture(
                asset_path,
                TextureWrap::REPEAT,
                TextureFilter::LINEAR,
                TextureFormat::RGBA,
                TextureMipmaps::ENABLED,
                TextureCompression::AUTO);
        }
    };

    template <>
    struct AssetLoader<AudioClip>
    {
        static AssetPromise<AudioClip> load_async(const std::filesystem::path& asset_path)
        {
            return load_audio_async(asset_path);
        }

        static std::shared_ptr<AudioClip> load(const std::filesystem::path& asset_path)
        {
            return load_audio(asset_path);
        }
    };

    template <>
    struct AssetLoader<Shader>
    {
        static AssetPromise<Shader> load_async(const std::filesystem::path& asset_path)
        {
            return load_shader_async(asset_path);
        }

        static std::shared_ptr<Shader> load(const std::filesystem::path& asset_path)
        {
            return load_shader(asset_path);
        }
    };

    template <>
    struct AssetLoader<Material>
    {
        static AssetPromise<Material> load_async(const std::filesystem::path& asset_path)
        {
            return load_material_async(asset_path);
        }

        static std::shared_ptr<Material> load(const std::filesystem::path& asset_path)
        {
            return load_material(asset_path);
        }
    };
}
