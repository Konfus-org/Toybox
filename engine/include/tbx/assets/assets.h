#pragma once
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/audio_clip.h"
#include "tbx/assets/material.h"
#include "tbx/assets/model.h"
#include "tbx/assets/script_source.h"
#include "tbx/assets/shader_source.h"
#include "tbx/assets/texture.h"
#include "tbx/assets/ui_document.h"
#include "tbx/core/result.h"
#include "tbx/core/typedefs.h"
#include "tbx/core/uuid.h"
#include "tbx/events/events.h"
#include "tbx/files/watcher.h"
#include "tbx/jobs/jobs.h"
#include "tbx/reflect/type_info.h"
#include <any>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace tbx
{
    /// @brief
    /// Purpose: Async-first asset loading over a static per-type decoder table, with .meta
    /// identity sidecars and watcher-driven hot reload. Paths resolve against the app's asset
    /// root first, then the engine's resources folder (TBX_RESOURCES_PATH) — so engine-shipped
    /// models/textures/shaders are ordinary assets too.
    /// @details
    /// Ownership: Owns decoded assets and the file watcher. Thread Safety: the identity and
    /// storage maps are mutex-guarded; load() decodes on a worker and stores on the main
    /// thread, load_now()/acquire() decode inline on the calling thread.
    class Assets final
    {
      public:
        Assets(Jobs& jobs, Events& events);

      public:
        Assets(const Assets&) = delete;
        Assets& operator=(const Assets&) = delete;

      public:
        /// @brief
        /// Purpose: The loaded asset for a handle, loading it synchronously by its tracked
        /// path when it is known but not resident — the renderer/material resolution path.
        template <typename TAsset>
        Result<std::reference_wrapper<TAsset>> acquire(AssetHandle<TAsset> handle);

        /// @brief
        /// Purpose: The loaded asset for a handle; empty if not (yet) loaded.
        template <typename TAsset>
        std::optional<std::reference_wrapper<TAsset>> get(AssetHandle<TAsset> handle);

        /// @brief
        /// Purpose: Loads (or returns the already-loaded) asset at a relative path: bytes read
        /// + decoded on a worker, stored on the main thread.
        template <typename TAsset>
        Task<Result<AssetHandle<TAsset>>> load(std::string relative_path);

        /// @brief
        /// Purpose: Synchronous load for startup/resolver paths: decodes inline on the calling
        /// thread and returns the handle.
        template <typename TAsset>
        Result<AssetHandle<TAsset>> load_now(const std::string& relative_path);

        /// @brief
        /// Purpose: A kit resolver over this asset system: kit/level references ("kits/x.kit")
        /// load as Json assets, so sandbox layouts stream straight from files.
        std::function<Result<Json>(const std::string&)> make_kit_resolver();

        /// @brief
        /// Purpose: Sets the asset root and starts watching it for hot reload.
        void set_root(std::filesystem::path root);

      private:
        /// @brief
        /// Purpose: One tracked asset: where it lives and how to re-decode it on change.
        struct Entry
        {
            Uuid id = {};
            std::string relative_path = {};
        };

      private:
        template <typename TAsset>
        Result<TAsset> decode(const std::filesystem::path& path);

        std::optional<std::string> find_relative_path(const Uuid& id);
        Result<Uuid> prepare(const std::string& relative_path); // .meta sidecar identity
        std::filesystem::path resolve_path(const std::string& relative_path) const;
        void store(const Uuid& id, const std::string& relative_path, std::any asset);
        void handle_file_changed(const std::filesystem::path& path);

      private:
        std::reference_wrapper<Jobs> _jobs;
        std::reference_wrapper<Events> _events;
        std::filesystem::path _root = {};
        mutable std::mutex _mutex; // guards _assets + _entries_by_path
        std::unordered_map<Uuid, std::any> _assets;
        std::unordered_map<std::string, Entry> _entries_by_path;
        std::optional<FileWatcher> _watcher = {};
    };

    template <>
    Result<Texture> Assets::decode<Texture>(const std::filesystem::path& path);
    template <>
    Result<ScriptSource> Assets::decode<ScriptSource>(const std::filesystem::path& path);
    template <>
    Result<Json> Assets::decode<Json>(const std::filesystem::path& path);
    template <>
    Result<Model> Assets::decode<Model>(const std::filesystem::path& path);
    template <>
    Result<ShaderSource> Assets::decode<ShaderSource>(const std::filesystem::path& path);
    template <>
    Result<AudioClip> Assets::decode<AudioClip>(const std::filesystem::path& path);
    template <>
    Result<Material> Assets::decode<Material>(const std::filesystem::path& path);
    template <>
    Result<UiDocument> Assets::decode<UiDocument>(const std::filesystem::path& path);
}

#include "tbx/assets/assets.inl"
