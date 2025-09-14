#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Files/WorkingDirectory.h"
#include <memory>

namespace Tbx
{
    class EXPORT ITextureLoader
    {
    public:
        virtual std::shared_ptr<Texture> LoadTexture(const std::string& filepath) = 0;
    };

    class EXPORT IShaderLoader
    {
    public:
        virtual std::shared_ptr<Shader> LoadShader(const std::string& filepath) = 0;
    };

    template<typename TAsset>
    using AssetLoadingFunc = std::function<std::shared_ptr<TAsset>(const std::string&)>;

    /// <summary>
    /// Loads an assets source data.
    /// </summary>
    template<typename TAsset>
    class AssetLoader
    {
    public:
        AssetLoader(AssetLoadingFunc<TAsset> loadFunc) 
            : _loadingFunc(loadFunc)
        {
        }

        /// <summary>
        /// Loads asset data of type AssetType from the specified filename.
        /// </summary>
        /// <typeparam name="AssetType">The type of asset to load</typeparam>
        /// <param name="filepath">The path on disk to the asset to load</param>
        /// <returns>An Asset object containing the loaded asset, or an nullptr if the load failed.</returns>
        EXPORT std::shared_ptr<TAsset> Load(const std::string& filepath)
        {
            // Ensure the given path is valid
            auto fsPath = std::filesystem::path(FileSystem::GetWorkingDirectory() + '/' + filepath);
            std::error_code ec;
            std::ignore = std::filesystem::status(fsPath, ec);
            TBX_ASSERT(!ec, "The asset path \"{}\" isn't valid: {}", fsPath.string(), ec.message());
            if (ec)
            {
                return nullptr;
            }

            auto asset = _loadingFunc(filepath);
            if (!asset)
            {
                TBX_ASSERT(false, "Loading an asset of type {} failed!", typeid(TAsset).name());
                return nullptr;
            }

            return asset;
        }

    private:
        AssetLoadingFunc<TAsset> _loadingFunc = {};
    };
}