#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/types/size.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace tbx::rpc_communication
{
    constexpr uint32 FRAME_MAGIC = 0x46584254U; // "TBXF"
    constexpr uint32 FRAME_FORMAT_VERSION = 1U;
    constexpr uint32 FRAME_SLOT_COUNT = 2U;
    constexpr uint32 FRAME_MAX_WIDTH = 3840U;
    constexpr uint32 FRAME_MAX_HEIGHT = 2160U;
    constexpr size FRAME_SLOT_CAPACITY = static_cast<size>(FRAME_MAX_WIDTH) * FRAME_MAX_HEIGHT * 4U;
    constexpr size FRAME_HEADER_SIZE = 4096U;

    struct SharedFrameSlot
    {
        uint32 sequence = 0U; // odd while the writer is mid-update
        uint32 width = 0U;
        uint32 height = 0U;
        uint32 stride = 0U;
        uint64 offset = 0U;
    };

    struct SharedFrameHeader
    {
        uint32 magic = 0U;
        uint32 version = 0U;
        uint32 slot_count = 0U;
        uint32 latest_slot = 0U;
        SharedFrameSlot slots[FRAME_SLOT_COUNT] = {};
    };

    /// @brief
    /// Purpose: Copies backend back-buffer readbacks into a double-buffered named shared memory
    /// region that the editor maps, using per-slot sequence counters so readers always get a
    /// complete frame.
    /// @details
    /// Ownership: Owns the file mapping. Thread Safety: start/stop are safe from the main thread;
    /// capture_backbuffer must run on the render lane.
    class FrameCapture
    {
      public:
        FrameCapture() = default;
        ~FrameCapture() noexcept;

      public:
        FrameCapture(const FrameCapture&) = delete;
        FrameCapture& operator=(const FrameCapture&) = delete;
        FrameCapture(FrameCapture&&) = delete;
        FrameCapture& operator=(FrameCapture&&) = delete;

      public:
        Result start(const std::string& name);
        void stop();
        bool is_active() const;
        void capture_backbuffer(tbx::IGraphicsBackend& backend, const tbx::Size& backbuffer_size);

      private:
        void write_frame(tbx::IGraphicsBackend& backend, const tbx::Size& backbuffer_size);

      private:
        mutable std::mutex _mutex = {};
        void* _mapping_handle = nullptr;
        uint8* _view = nullptr;
        bool _is_active = false;
        uint32 _next_slot = 0U;
        std::vector<uint8> _staging = {};
        std::chrono::steady_clock::time_point _last_capture_time = {};
    };
}
