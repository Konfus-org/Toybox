#pragma once
#include "tbx/core/api.h"
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
    class TBX_API Assets final
    {
      public:
        Assets(Jobs& jobs, Events& events);

      public:
        Assets(const Assets&) = delete;
        Assets& operator=(const Assets&) = delete;

      public:
        /// @brief
        /// Purpose: Number of resident (decoded) assets — debug/tooling.
        size get_loaded_count() const;

        /// @brief
        /// Purpose: The asset a handle references, loading it asynchronously when it is not
        /// resident: bytes read + decoded on a worker, stored on the main thread.
        template <typename TAsset>
        Task<Result<std::reference_wrapper<TAsset>>> load(AssetHandle<TAsset> handle);

        /// @brief
        /// Purpose: The asset a handle references, decoded inline on the calling thread when
        /// it is not already resident — the renderer/material/startup resolution path.
        template <typename TAsset>
        Result<std::reference_wrapper<TAsset>> load_now(AssetHandle<TAsset> handle);

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
        /// @brief
        /// Purpose: A handle after identity resolution: the id plus the path to decode from.
        struct ResolvedHandle
        {
            Uuid id = {};
            std::string relative_path = {};
        };

      private:
        template <typename TAsset>
        Result<TAsset> decode(const std::filesystem::path& path);

        template <typename TAsset>
        std::optional<std::reference_wrapper<TAsset>> find_resident(const Uuid& id);

        Result<ResolvedHandle> resolve_handle(const Uuid& id, const std::string& path);
        std::optional<std::string> find_relative_path(const Uuid& id);
        void index_meta_sidecars(); // caller holds _mutex
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
        bool _is_indexed = false;
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
