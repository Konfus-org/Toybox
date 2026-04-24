#include "pch.h"
#include "tbx/core/systems/files/tests/in_memory_file_ops.h"
#include "tbx/systems/files/watcher.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>


namespace tbx::tests::file_system
{
    struct CapturedFileWatchChange
    {
        std::filesystem::path watched_path = {};
        std::filesystem::path changed_path = {};
        FileWatchChangeType change_type = FileWatchChangeType::MODIFIED;
    };

    class CapturingFileWatchAction final
    {
      public:
        void operator()(const std::filesystem::path& watched_path, const FileWatchChange& change)
        {
            {
                std::lock_guard<std::mutex> lock(_changes_mutex);
                _changes.emplace_back(
                    CapturedFileWatchChange {
                        .watched_path = watched_path,
                        .changed_path = change.path,
                        .change_type = change.type,
                    });
            }

            _changes_signal.notify_all();
        }

      public:
        std::vector<CapturedFileWatchChange> get_changes() const
        {
            std::lock_guard<std::mutex> lock(_changes_mutex);
            return _changes;
        }

        bool wait_for_count(
            std::vector<CapturedFileWatchChange>::size_type count,
            std::chrono::milliseconds timeout) const
        {
            std::unique_lock<std::mutex> lock(_changes_mutex);
            return _changes_signal.wait_for(
                lock,
                timeout,
                [this, count]()
                {
                    return _changes.size() >= count;
                });
        }

      private:
        mutable std::mutex _changes_mutex = {};
        mutable std::condition_variable _changes_signal = {};
        mutable std::vector<CapturedFileWatchChange> _changes = {};
    };

    TEST(FileWatcherTests, DiffsCreatedModifiedAndRemovedFiles)
    {
        const auto base_time = std::filesystem::file_time_type::clock::now();
        const FileWatchSnapshot previous_snapshot = {
            {std::filesystem::path("assets/a.txt"), base_time},
            {std::filesystem::path("assets/b.txt"), base_time},
        };
        const FileWatchSnapshot current_snapshot = {
            {std::filesystem::path("assets/a.txt"), base_time + std::chrono::seconds(1)},
            {std::filesystem::path("assets/c.txt"), base_time},
        };

        const std::vector<FileWatchChange> changes =
            diff_file_watch_snapshots(previous_snapshot, current_snapshot);

        ASSERT_EQ(changes.size(), 3u);
        EXPECT_EQ(changes[0].path, std::filesystem::path("assets/a.txt"));
        EXPECT_EQ(changes[0].type, FileWatchChangeType::MODIFIED);
        EXPECT_EQ(changes[1].path, std::filesystem::path("assets/b.txt"));
        EXPECT_EQ(changes[1].type, FileWatchChangeType::REMOVED);
        EXPECT_EQ(changes[2].path, std::filesystem::path("assets/c.txt"));
        EXPECT_EQ(changes[2].type, FileWatchChangeType::CREATED);
    }

    TEST(FileWatcherTests, InvokesChangeActionForFilesWithinWatchedDirectory)
    {
        const auto base_time = std::filesystem::file_time_type::clock::now();
        auto file_ops = std::make_shared<InMemoryFileOps>("/virtual");
        file_ops->write_file_entry("assets/a.txt", {}, base_time);
        file_ops->write_file_entry("assets/b.txt", {}, base_time);
        CapturingFileWatchAction on_changed = {};

        FileWatcher watcher(
            "assets",
            [&on_changed](const std::filesystem::path& watched_path, const FileWatchChange& change)
            {
                on_changed(watched_path, change);
            },
            std::chrono::milliseconds(5),
            file_ops);

        file_ops->touch("assets/a.txt", base_time + std::chrono::seconds(1));
        file_ops->erase("assets/b.txt");
        file_ops->write_file_entry("assets/c.txt", {}, base_time);

        ASSERT_TRUE(on_changed.wait_for_count(3, std::chrono::milliseconds(500)));

        std::vector<CapturedFileWatchChange> changes = on_changed.get_changes();
        ASSERT_EQ(changes.size(), 3u);

        std::sort(
            changes.begin(),
            changes.end(),
            [](const CapturedFileWatchChange& lhs, const CapturedFileWatchChange& rhs)
            {
                return lhs.changed_path.generic_string() < rhs.changed_path.generic_string();
            });

        EXPECT_EQ(changes[0].watched_path, std::filesystem::path("assets"));
        EXPECT_EQ(changes[0].changed_path, std::filesystem::path("assets/a.txt"));
        EXPECT_EQ(changes[0].change_type, FileWatchChangeType::MODIFIED);

        EXPECT_EQ(changes[1].watched_path, std::filesystem::path("assets"));
        EXPECT_EQ(changes[1].changed_path, std::filesystem::path("assets/b.txt"));
        EXPECT_EQ(changes[1].change_type, FileWatchChangeType::REMOVED);

        EXPECT_EQ(changes[2].watched_path, std::filesystem::path("assets"));
        EXPECT_EQ(changes[2].changed_path, std::filesystem::path("assets/c.txt"));
        EXPECT_EQ(changes[2].change_type, FileWatchChangeType::CREATED);
    }
}
