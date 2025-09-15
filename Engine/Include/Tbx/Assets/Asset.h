#pragma once
#include "Tbx/Ids/UsesGUID.h"
#include "Tbx/Assets/AssetLoaders.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    /// A generic asset wrapper capable of holding any type of data.
    /// Supports runtime type checking and safe retrieval of typed data.
    /// </summary>
    template<typename TAsset>
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
        EXPORT Asset(const std::string& filepath, std::shared_ptr<AssetLoader<TAsset>> loader)
            : UsesGuid()
            , _name(std::filesystem::path(filepath).filename().string())
            , _filepath(filepath)
            , _loader(loader)
        {
        }

        /// <summary>
        /// Lazy loads data from disk and puts the data into a shared pointer.
        /// The data only lives as long as the returned shared pointer is alive.
        /// </summary>
        /// <typeparam name="T">The expected type of the stored data.</typeparam>
        /// <returns>A shared pointer to the data.</returns>
        EXPORT std::shared_ptr<TAsset> GetData() const
        {
            return _loader->Load(_filepath);
        }

        /// <summary>
        /// Gets the name of the asset.
        /// </summary>
        EXPORT const std::string& GetName() const 
        {
            return _name;
        }

    private:
        std::string _name = "NONE";
        std::string _filepath = "";
        std::shared_ptr<AssetLoader<TAsset>> _loader = nullptr;
    };
}
