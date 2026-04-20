#pragma once
#include "tbx/files/events.h"
#include "tbx/files/ops.h"
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace tbx
{
    using FileWatchSnapshot =
        std::unordered_map<std::filesystem::path, std::filesystem::file_time_type>;
    using FileWatchAction =
        std::function<void(const std::filesystem::path&, const FileWatchChange&)>;

    /// @brief
    /// Purpose: Computes file changes between two filesystem snapshots.
    /// @details
    /// Ownership: Returns a caller-owned list of change records.
    /// Thread Safety: Safe to call concurrently.
    TBX_API std::vector<FileWatchChange> diff_file_watch_snapshots(
        const FileWatchSnapshot& previous_snapshot,
        const FileWatchSnapshot& current_snapshot);

    /// @brief
    /// Purpose: Watches file(s) at a given path and invokes callbacks when a file changes.
    /// @details
    /// Ownership: Owns the polling thread and stops watching on destruction.
    /// Thread Safety: Safe to destroy while the background watch thread is running.
    class TBX_API FileWatcher final
    {
      public:
        /// @brief
        /// Purpose: Starts watching a file or directory path immediately.
        /// @details
        /// Ownership: Stores normalized copies of the path and callback and owns the polling
        /// thread. Borrows or owns the provided file-ops implementation through a shared pointer.
        /// When a directory is watched, all files beneath it are tracked recursively.
        FileWatcher(
            std::filesystem::path path_to_watch,
            FileWatchAction on_changed,
            std::chrono::milliseconds poll_interval = std::chrono::milliseconds(250),
            std::shared_ptr<IFileOps> file_ops = {});
        ~FileWatcher() noexcept;

      public:
        FileWatcher(const FileWatcher&) = delete;
        FileWatcher& operator=(const FileWatcher&) = delete;
        FileWatcher(FileWatcher&&) = delete;
        FileWatcher& operator=(FileWatcher&&) = delete;

      private:
        void notify_changes(const std::vector<FileWatchChange>& changes) const;
        void poll_watched_path();
        void run(std::stop_token stop_token);

      private:
        std::jthread _worker = {};
        mutable std::mutex _wait_mutex = {};
        std::condition_variable_any _wake_signal = {};

        FileWatchAction _on_changed = {};
        std::shared_ptr<IFileOps> _file_ops = {};
        FileWatchSnapshot _snapshot = {};
        std::filesystem::path _watched_path = {};
        std::chrono::milliseconds _poll_interval = std::chrono::milliseconds(250);
    };
}
