#include "tbx/systems/files/watcher.h"
#include "systems/files/internal/watcher_internal.h"
#include <memory>
#include <utility>

namespace tbx
{
    std::vector<FileWatchChange> diff_file_watch_snapshots(
        const FileWatchSnapshot& previous_snapshot,
        const FileWatchSnapshot& current_snapshot)
    {
        std::vector<FileWatchChange> changes = {};
        changes.reserve(previous_snapshot.size() + current_snapshot.size());

        for (const auto& [path, last_write_time] : current_snapshot)
        {
            const auto previous = previous_snapshot.find(path);
            if (previous == previous_snapshot.end())
            {
                changes.emplace_back(
                    FileWatchChange {
                        .path = path,
                        .type = FileWatchChangeType::CREATED,
                    });
                continue;
            }

            if (previous->second != last_write_time)
            {
                changes.emplace_back(
                    FileWatchChange {
                        .path = path,
                        .type = FileWatchChangeType::MODIFIED,
                    });
            }
        }

        for (const auto& [path, _] : previous_snapshot)
        {
            if (current_snapshot.contains(path))
                continue;

            changes.emplace_back(
                FileWatchChange {
                    .path = path,
                    .type = FileWatchChangeType::REMOVED,
                });
        }

        internal::sort_changes(changes);
        return changes;
    }

    FileWatcher::FileWatcher(
        std::filesystem::path path_to_watch,
        FileWatchAction on_changed,
        std::chrono::milliseconds poll_interval,
        std::shared_ptr<IFileOps> file_ops)
        : FileWatcher(
              std::move(path_to_watch),
              std::move(on_changed),
              internal::make_fixed_file_watch_options(poll_interval),
              std::move(file_ops))
    {
    }

    FileWatcher::FileWatcher(
        std::filesystem::path path_to_watch,
        FileWatchAction on_changed,
        FileWatchOptions options,
        std::shared_ptr<IFileOps> file_ops)
        : _on_changed(std::move(on_changed))
        , _file_ops(std::move(file_ops))
        , _snapshot({})
        , _options(internal::normalize_file_watch_options(std::move(options)))
        , _watched_path(path_to_watch.lexically_normal())
    {
        if (!_file_ops)
            _file_ops = std::make_shared<FileOperator>();
        if (_watched_path.empty() || !_on_changed)
            return;

        _snapshot = internal::read_snapshot(*_file_ops, _watched_path, _options.filter);

        _worker = std::jthread(
            [this](std::stop_token stop_token)
            {
                run(stop_token);
            });
    }

    FileWatcher::~FileWatcher() noexcept
    {
        if (_worker.joinable())
        {
            _worker.request_stop();
            _wake_signal.notify_all();
            _worker.join();
        }
    }

    void FileWatcher::notify_changes(const std::vector<FileWatchChange>& changes) const
    {
        for (const FileWatchChange& change : changes)
        {
            _on_changed(_watched_path, change);
        }
    }

    std::chrono::milliseconds FileWatcher::get_next_poll_interval() const
    {
        if (_options.idle_poll_interval <= _options.active_poll_interval)
            return _options.active_poll_interval;
        if (_unchanged_scan_count < _options.unchanged_scan_threshold)
            return _options.active_poll_interval;

        return _options.idle_poll_interval;
    }

    bool FileWatcher::poll_watched_path()
    {
        if (_watched_path.empty() || !_on_changed)
            return false;

        FileWatchSnapshot current_snapshot =
            internal::read_snapshot(*_file_ops, _watched_path, _options.filter);
        const std::vector<FileWatchChange> changes =
            diff_file_watch_snapshots(_snapshot, current_snapshot);

        if (!changes.empty())
            notify_changes(changes);

        _snapshot = std::move(current_snapshot);
        if (changes.empty())
        {
            _unchanged_scan_count += 1U;
            return false;
        }

        _unchanged_scan_count = 0U;
        return true;
    }

    void FileWatcher::run(std::stop_token stop_token)
    {
        std::unique_lock<std::mutex> wait_lock(_wait_mutex);
        while (!stop_token.stop_requested())
        {
            wait_lock.unlock();
            poll_watched_path();
            wait_lock.lock();

            if (stop_token.stop_requested())
                break;

            _wake_signal.wait_for(wait_lock, get_next_poll_interval());
        }
    }
}
