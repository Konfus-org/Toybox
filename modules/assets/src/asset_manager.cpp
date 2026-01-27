#include "tbx/assets/asset_manager.h"

namespace tbx
{
    AssetManager::AssetManager(IFileSystem& file_system)
        : _file_system(&file_system)
        , _assets_root(file_system.get_assets_directory())
    {
        discover_assets();
    }

    void AssetManager::clean()
    {
        std::lock_guard lock(_mutex);
        for (const auto& store : _stores)
        {
            store.second->clean();
        }
    }

    void AssetManager::discover_assets()
    {
        if (!_file_system)
        {
            return;
        }
        if (_assets_root.empty())
        {
            _assets_root = _file_system->get_assets_directory();
        }
        if (_assets_root.empty())
        {
            return;
        }
        const auto entries = _file_system->read_directory(_assets_root);
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
            static_cast<void>(get_or_create_registry_entry(normalized));
        }
    }
