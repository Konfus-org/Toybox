#include "frame_capture.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#define NOMINMAX
#include <windows.h>

namespace tbx::rpc_communication
{
    // Cap streamed frames at ~60Hz: the editor view and typical displays refresh at 60, and the
    // readback is now asynchronous (no GPU stall), so there is no reason to throttle harder.
    constexpr std::chrono::microseconds CAPTURE_INTERVAL(16667);

    static void store_release_u32(uint32& target, uint32 value)
    {
        std::atomic_ref<uint32>(target).store(value, std::memory_order_release);
    }

    FrameCapture::~FrameCapture() noexcept
    {
        stop();
    }

    Result FrameCapture::start(const std::string& name)
    {
        auto lock = std::lock_guard(_mutex);
        if (_is_active)
            return Result::OK;

        const auto total_size = FRAME_HEADER_SIZE + FRAME_SLOT_COUNT * FRAME_SLOT_CAPACITY;
        auto mapping = ::CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            static_cast<DWORD>(total_size >> 32U),
            static_cast<DWORD>(total_size & 0xFFFFFFFFU),
            name.c_str());
        if (mapping == nullptr)
            return Result(false, "Failed to create the shared frame buffer mapping.");

        auto* view = ::MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (view == nullptr)
        {
            ::CloseHandle(mapping);
            return Result(false, "Failed to map the shared frame buffer view.");
        }

        auto* header = static_cast<SharedFrameHeader*>(view);
        *header = SharedFrameHeader();
        header->magic = FRAME_MAGIC;
        header->version = FRAME_FORMAT_VERSION;
        header->slot_count = FRAME_SLOT_COUNT;
        for (uint32 slot = 0U; slot < FRAME_SLOT_COUNT; ++slot)
            header->slots[slot].offset = FRAME_HEADER_SIZE + slot * FRAME_SLOT_CAPACITY;

        _mapping_handle = mapping;
        _view = static_cast<uint8*>(view);
        _next_slot = 0U;
        _last_capture_time = {};
        _is_active = true;
        return Result::OK;
    }

    void FrameCapture::stop()
    {
        auto lock = std::lock_guard(_mutex);
        if (_view != nullptr)
        {
            ::UnmapViewOfFile(_view);
            _view = nullptr;
        }

        if (_mapping_handle != nullptr)
        {
            ::CloseHandle(_mapping_handle);
            _mapping_handle = nullptr;
        }

        _is_active = false;
    }

    bool FrameCapture::is_active() const
    {
        auto lock = std::lock_guard(_mutex);
        return _is_active;
    }

    void FrameCapture::capture_backbuffer(
        tbx::IGraphicsBackend& backend,
        const tbx::Size& backbuffer_size)
    {
        auto lock = std::lock_guard(_mutex);
        if (!_is_active || _view == nullptr)
            return;

        const auto now = std::chrono::steady_clock::now();
        if (now - _last_capture_time < CAPTURE_INTERVAL)
            return;
        _last_capture_time = now;

        write_frame(backend, backbuffer_size);
    }

    void FrameCapture::write_frame(
        tbx::IGraphicsBackend& backend,
        const tbx::Size& backbuffer_size)
    {
        auto clamped_size = tbx::Size();
        clamped_size.width = std::min(backbuffer_size.width, FRAME_MAX_WIDTH);
        clamped_size.height = std::min(backbuffer_size.height, FRAME_MAX_HEIGHT);
        if (clamped_size.width == 0U || clamped_size.height == 0U)
            return;

        if (!backend.read_back_buffer(clamped_size, _staging))
            return;

        const auto stride = clamped_size.width * 4U;
        auto* header = reinterpret_cast<SharedFrameHeader*>(_view);
        auto& slot = header->slots[_next_slot];
        store_release_u32(slot.sequence, slot.sequence + 1U); // odd: write in progress

        std::memcpy(
            _view + slot.offset,
            _staging.data(),
            static_cast<size>(stride) * clamped_size.height);

        slot.width = clamped_size.width;
        slot.height = clamped_size.height;
        slot.stride = stride;
        store_release_u32(slot.sequence, slot.sequence + 1U); // even: stable
        store_release_u32(header->latest_slot, _next_slot);
        _next_slot = (_next_slot + 1U) % FRAME_SLOT_COUNT;
    }
}
