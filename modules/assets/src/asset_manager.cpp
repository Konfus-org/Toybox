#include "tbx/assets/asset_manager.h"

namespace tbx
{
    AssetManager::AssetManager(IFileSystem& file_system)
        : _file_system(&file_system)
    {
        discover_assets();
    }

    void AssetManager::unload_all()
    {
        _stores.clear();
    }

    void AssetManager::unload_unreferenced()
    {
        std::lock_guard lock(_mutex);
        for (const auto& store : _stores)
        {
            store.second->unload_unreferenced();
        }
    }

    void AssetManager::discover_assets()
    {
        if (!_file_system)
        {
            return;
        }
        const auto roots = _file_system->get_assets_directories();
        for (const auto& root : roots)
        {
            if (root.empty())
            {
                continue;
            }
            const auto entries = _file_system->read_directory(root);
            for (const auto& entry : entries)
            {
                if (_file_system->get_file_type(entry) != FilePathType::Regular)
                {
                    continue;
                }
                if (entry.extension() == ".meta")
                {
                    continue;
                }
                const auto normalized = normalize_path(entry);
                if (_registry.contains(normalized.path_key))
                {
                    continue;
                }
                get_or_create_registry_entry(normalized);
            }
        }
    }
}
