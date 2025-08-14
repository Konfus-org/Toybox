#pragma once
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Ids/UsesGUID.h"
#include "Tbx/PluginAPI/PluginServer.h"
#include "Tbx/PluginAPI/PluginInterfaces.h"
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <stdexcept>

namespace Tbx
{
    /// <summary>
    /// Loads an assets source data.
    /// </summary>
    class AssetLoader
    {
    public:
        /// <summary>
        /// Loads asset data of type AssetType from the specified filename.
        /// </summary>
        /// <typeparam name="AssetType">The type of asset to load</typeparam>
        /// <param name="filepath">The path on disk to the asset to load</param>
        /// <returns>An Asset object containing the loaded asset, or an empty Asset object if the load failed.</returns>
        template <typename AssetType>
        EXPORT static std::shared_ptr<AssetType> Load(const std::string& filepath)
        {
            auto fsPath = std::filesystem::path(filepath);
            if constexpr (std::is_same<Shader, AssetType>())
            {
                auto shaderLoader = GetShaderLoader();
                TBX_ASSERT(shaderLoader, "Shader loading plugin not found!");
                return shaderLoader->LoadShader(filepath);
            }
            else if constexpr (std::is_same<Texture, AssetType>())
            {
                auto textureLoader = GetTextureLoader();
                TBX_ASSERT(textureLoader, "Texture loading plugin not found!");
                return textureLoader->LoadTexture(filepath);
            }
            else
            {
                TBX_ASSERT(false, "Loading an asset of type {} failed! This means this type of asset is currently unsupported.", typeid(AssetType).name());
                return {};
            }
        }

    private:
        EXPORT static std::shared_ptr<IShaderLoaderPlugin> GetShaderLoader();
        EXPORT static std::shared_ptr<ITextureLoaderPlugin> GetTextureLoader();
    };

    /// <summary>
    /// A generic asset wrapper capable of holding any type of data.
    /// Supports runtime type checking and safe retrieval of typed data.
    /// </summary>
    template<typename T>
    class Asset : public UsesGuid
    {
    public:
        /// <summary>
        /// Default constructor. Creates an empty asset.
        /// </summary>
        EXPORT Asset() = default;

        /// <summary>
        /// Constructs an asset from a typed shared pointer.
        /// </summary>
        /// <typeparam name="T">The type of the data to store.</typeparam>
        /// <param name="data">The shared pointer to the data.</param>
        EXPORT Asset(const std::string& filepath)
            : UsesGuid()
            , _name(std::filesystem::path(filepath).filename().string())
            , _filepath(filepath)
        {
        }

        /// <summary>
        /// Lazy loads data from disk and puts the data into a shared pointer.
        /// The data only lives as long as the returned shared pointer is alive.
        /// </summary>
        /// <typeparam name="T">The expected type of the stored data.</typeparam>
        /// <returns>A shared pointer to the data.</returns>
        EXPORT std::shared_ptr<T> GetData() const
        {
            return AssetLoader::Load<T>(_filepath);
        }

        /// <summary>
        /// Gets the name of the asset.
        /// </summary>
        EXPORT const std::string& GetName() const 
        {
            return _name;
        }

    private:
        std::string _filepath = "";
        std::string _name = "NONE";
    };
}
