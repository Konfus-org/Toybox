#pragma once
#include <chrono>
#include <filesystem>
#include <functional>
#include <thread>
#include <unordered_map>

namespace tbx
{
    /// @brief
    /// Purpose: Watches a directory tree and reports changed/added files by polling mtimes —
    /// drives script and asset hot reload.
    /// @details
    /// Ownership: Owns the polling thread; stops on destruction (RAII). Thread Safety: The
    /// callback runs ON THE WATCHER THREAD — callers marshal to the main thread themselves
    /// (e.g. via jobs::post_main).
    class FileWatcher final
    {
      public:
        FileWatcher(
            std::filesystem::path root,
            std::function<void(const std::filesystem::path&)> on_changed,
            std::chrono::milliseconds poll_interval = std::chrono::milliseconds(250));
        ~FileWatcher() = default; // jthread stops + joins

      public:
        FileWatcher(const FileWatcher&) = delete;
        FileWatcher& operator=(const FileWatcher&) = delete;

      private:
        void poll_loop(std::stop_token stop);
        void scan(bool report);

      private:
        std::filesystem::path _root;
        std::function<void(const std::filesystem::path&)> _on_changed;
        std::chrono::milliseconds _poll_interval;
        std::unordered_map<std::string, std::filesystem::file_time_type> _snapshot;
        std::jthread _thread; // declared last: starts after state above is ready
    };
}
