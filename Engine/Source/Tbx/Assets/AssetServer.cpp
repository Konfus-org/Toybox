#include "Tbx/PCH.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Files/Paths.h"
#include <array>
#include <typeindex>

namespace Tbx
{
    struct AssetLoaderBinding
    {
        std::type_index AssetType = std::type_index(typeid(void));
        bool (*Matches)(const Ref<IAssetLoader>& loader) = nullptr;
    };

    static bool IsTextureLoader(const Ref<IAssetLoader>& loader)
    {
        return std::dynamic_pointer_cast<ITextureLoader>(loader) != nullptr;
    }

    static bool IsShaderLoader(const Ref<IAssetLoader>& loader)
    {
        return std::dynamic_pointer_cast<IShaderLoader>(loader) != nullptr;
    }

    static bool IsModelLoader(const Ref<IAssetLoader>& loader)
    {
        return std::dynamic_pointer_cast<IModelLoader>(loader) != nullptr;
    }

    static bool IsTextLoader(const Ref<IAssetLoader>& loader)
    {
        return std::dynamic_pointer_cast<ITextLoader>(loader) != nullptr;
    }

    static bool IsAudioLoader(const Ref<IAssetLoader>& loader)
    {
        return std::dynamic_pointer_cast<IAudioLoader>(loader) != nullptr;
    }

    static constexpr std::array<AssetLoaderBinding, 5> AssetLoaderBindings = {
        AssetLoaderBinding{ std::type_index(typeid(TextureAsset)), &IsTextureLoader },
        AssetLoaderBinding{ std::type_index(typeid(ShaderAsset)), &IsShaderLoader },
        AssetLoaderBinding{ std::type_index(typeid(ModelAsset)), &IsModelLoader },
        AssetLoaderBinding{ std::type_index(typeid(TextAsset)), &IsTextLoader },
        AssetLoaderBinding{ std::type_index(typeid(AudioAsset)), &IsAudioLoader }
    };

    AssetServer::AssetServer(const std::string& assetsFolderPath, const std::vector<Ref<IAssetLoader>>& loaders)
        : _assetDirectory(std::filesystem::absolute(assetsFolderPath))
        , _loaders(loaders)
    {
        BuildLoaderLookup();
    }

    void AssetServer::BuildLoaderLookup()
    {
        _loadersByType.clear();

        for (const auto& loader : _loaders)
        {
            if (!loader)
            {
                continue;
            }

            for (const auto& binding : AssetLoaderBindings)
            {
                if (binding.Matches && binding.Matches(loader))
                {
                    _loadersByType[binding.AssetType].push_back(loader);
                }
            }
        }
    }

    Ref<IAssetLoader> AssetServer::FindLoaderFor(const std::filesystem::path& filePath) const
    {
        for (const auto& loader : _loaders)
        {
            if (!loader)
            {
                continue;
            }

            if (loader->CanLoad(filePath))
            {
                return loader;
            }
        }

        return nullptr;
    }

    Ref<IAssetLoader> AssetServer::FindLoaderForType(std::type_index assetType, const std::filesystem::path& filePath) const
    {
        auto loaderIt = _loadersByType.find(assetType);
        if (loaderIt == _loadersByType.end())
        {
            return nullptr;
        }

        for (const auto& loader : loaderIt->second)
        {
            if (!loader)
            {
                continue;
            }

            if (loader->CanLoad(filePath))
            {
                return loader;
            }
        }

        return nullptr;
    }

    std::filesystem::path AssetServer::ResolvePath(const std::filesystem::path& path) const
    {
        if (path.is_absolute())
        {
            return path;
        }

        if (auto combined = _assetDirectory / path;
            std::filesystem::exists(combined))
        {
            return combined;
        }

        auto absolute = std::filesystem::absolute(path);
        if (!std::filesystem::exists(absolute))
        {
            TBX_ASSERT(false, "AssetServer: Path couldn't be resolved! Does it exist?");
            return "";
        }
        return absolute;
    }

    std::string AssetServer::NormalizeKey(const std::filesystem::path& path) const
    {
        auto absolutePath = ResolvePath(path);
        auto relativeString = FileSystem::GetRelativePath(absolutePath);
        return FileSystem::NormalizePath(relativeString);
    }
}
