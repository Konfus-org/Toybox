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

    static const std::array AssetLoaderBindings =
    {
        AssetLoaderBinding{ std::type_index(typeid(Texture)), &IsTextureLoader },
        AssetLoaderBinding{ std::type_index(typeid(Shader)), &IsShaderLoader },
        AssetLoaderBinding{ std::type_index(typeid(Model)), &IsModelLoader },
        AssetLoaderBinding{ std::type_index(typeid(Text)), &IsTextLoader },
        AssetLoaderBinding{ std::type_index(typeid(Audio)), &IsAudioLoader }
    };

    AssetServer::AssetServer(const std::string& assetsFolderPath, const std::vector<Ref<IAssetLoader>>& loaders)
        : _assetDirectory(std::filesystem::absolute(assetsFolderPath))
    {
        BuildLoaderLookup(loaders);
    }

    void AssetServer::BuildLoaderLookup(const std::vector<Ref<IAssetLoader>>& loaders)
    {
        _loadersByType.clear();

        for (const auto& loader : loaders)
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
