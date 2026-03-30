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
        using Parameters = DefaultAssetLoadParameters;

        static AssetPromise<TAsset> load_async(
            const std::filesystem::path& asset_path,
            const Parameters& parameters = {})
        {
            static_assert(sizeof(TAsset) == 0, "No async asset loader registered for this type.");
            return load_async(asset_path, parameters);
        }

        static std::shared_ptr<TAsset> load(
            const std::filesystem::path& asset_path,
            const Parameters& parameters = {})
        {
            static_assert(sizeof(TAsset) == 0, "No asset loader registered for this type.");
            return load(asset_path, parameters);
        }
    };

    template <>
    struct AssetLoader<Model>
    {
        using Parameters = ModelLoadParameters;

        static AssetPromise<Model> load_async(
            const std::filesystem::path& asset_path,
            const Parameters& parameters = {})
        {
            return load_model_async(asset_path, parameters);
        }

        static std::shared_ptr<Model> load(
            const std::filesystem::path& asset_path,
            const Parameters& parameters = {})
        {
            return load_model(asset_path, parameters);
        }
    };

    template <>
    struct AssetLoader<Texture>
    {
        using Parameters = TextureLoadParameters;

        static AssetPromise<Texture> load_async(
            const std::filesystem::path& asset_path,
            const Parameters& parameters = {})
        {
            return load_texture_async(asset_path, parameters);
        }

        static std::shared_ptr<Texture> load(
            const std::filesystem::path& asset_path,
            const Parameters& parameters = {})
        {
            return load_texture(asset_path, parameters);
        }
    };

    template <>
    struct AssetLoader<AudioClip>
    {
        using Parameters = AudioLoadParameters;

        static AssetPromise<AudioClip> load_async(
            const std::filesystem::path& asset_path,
            const Parameters& parameters = {})
        {
            return load_audio_async(asset_path, parameters);
        }

        static std::shared_ptr<AudioClip> load(
            const std::filesystem::path& asset_path,
            const Parameters& parameters = {})
        {
            return load_audio(asset_path, parameters);
        }
    };

    template <>
    struct AssetLoader<Shader>
    {
        using Parameters = ShaderLoadParameters;

        static AssetPromise<Shader> load_async(
            const std::filesystem::path& asset_path,
            const Parameters& parameters = {})
        {
            return load_shader_async(asset_path, parameters);
        }

        static std::shared_ptr<Shader> load(
            const std::filesystem::path& asset_path,
            const Parameters& parameters = {})
        {
            return load_shader(asset_path, parameters);
        }
    };

    template <>
    struct AssetLoader<Material>
    {
        using Parameters = MaterialLoadParameters;

        static AssetPromise<Material> load_async(
            const std::filesystem::path& asset_path,
            const Parameters& parameters = {})
        {
            return load_material_async(asset_path, parameters);
        }

        static std::shared_ptr<Material> load(
            const std::filesystem::path& asset_path,
            const Parameters& parameters = {})
        {
            return load_material(asset_path, parameters);
        }
    };
}
