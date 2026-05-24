#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace tbx::internal
{
    class ThreadLane final
    {
      public:
        using Task = std::move_only_function<void()>;

      public:
        ThreadLane(std::string lane_name);
        ~ThreadLane() noexcept;

      public:
        ThreadLane(const ThreadLane&) = delete;
        ThreadLane& operator=(const ThreadLane&) = delete;
        ThreadLane(ThreadLane&&) = delete;
        ThreadLane& operator=(ThreadLane&&) = delete;

      public:
        void post(Task&& task);
        void stop();

      private:
        void run(std::stop_token stop_token);
        void set_name(const std::string& lane_name);

      private:
        std::string _name = {};
        std::jthread _worker = {};
        std::mutex _queue_mutex = {};
        std::condition_variable _queued_task_signal = {};
        std::deque<Task> _queued_tasks = {};
        bool _accepting_tasks = true;
    };
}
