#include "tbx/files/watcher.h"

namespace tbx
{
    //// FILE WATCHER ////

    FileWatcher::FileWatcher(
        std::filesystem::path root,
        std::function<void(const std::filesystem::path&)> on_changed,
        std::chrono::milliseconds poll_interval)
        : _root(std::move(root))
        , _on_changed(std::move(on_changed))
        , _poll_interval(poll_interval)
        , _thread([this](std::stop_token stop) { poll_loop(stop); })
    {
    }

    void FileWatcher::scan(bool report)
    {
        auto ec = std::error_code {};
        auto iterator = std::filesystem::recursive_directory_iterator(_root, ec);
        if (ec)
            return;
        for (const auto& entry : iterator)
        {
            if (!entry.is_regular_file(ec))
                continue;
            const auto key = entry.path().string();
            const auto stamp = entry.last_write_time(ec);
            if (ec)
                continue;
            const auto it = _snapshot.find(key);
            const bool is_new_or_changed = it == _snapshot.end() || it->second != stamp;
            _snapshot[key] = stamp;
            if (report && is_new_or_changed)
                _on_changed(entry.path());
        }
    }

    void FileWatcher::poll_loop(std::stop_token stop)
    {
        scan(false); // baseline: existing files are not "changes"
        while (!stop.stop_requested())
        {
            std::this_thread::sleep_for(_poll_interval);
            if (stop.stop_requested())
                return;
            scan(true);
        }
    }
}
