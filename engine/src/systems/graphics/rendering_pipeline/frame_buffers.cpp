#include "frame_buffers.h"
#include <algorithm>

namespace tbx
{
    FrameBuffers::FrameBuffers(std::weak_ptr<IGraphicsBackend> backend)
        : _backend(std::move(backend))
    {
    }

    void FrameBuffers::begin()
    {
        _cursor = 0U;
    }

    GpuId FrameBuffers::store(const void* data, const uint64 size, const BufferUsage usage)
    {
        const auto backend = _backend.lock();
        if (!backend)
            return INVALID_GPU_ID;

        // Hand out pool slots in call order; grow the pool when a frame uses more buffers than any
        // before it.
        if (_cursor >= _slots.size())
            _slots.emplace_back();
        Slot& slot = _slots[_cursor++];

        const uint64 needed = std::max<uint64>(size, 1U);
        const BufferUsage full_usage = usage | BufferUsage::COPY_DST;
        // Reuse the slot's buffer when it still fits and matches the requested usage; otherwise
        // (re)create it. A grown buffer is kept at its larger capacity so later frames stop growing.
        if (!slot.buffer.is_valid() || slot.capacity < needed || slot.usage != full_usage)
        {
            auto desc = BufferDesc {.usage = full_usage, .size = needed, .is_dynamic = true};
            auto id = INVALID_GPU_ID;
            if (auto result = backend->create_buffer(desc, id); !result)
                return INVALID_GPU_ID;
            slot.buffer = GpuResource(_backend, id);
            slot.capacity = needed;
            slot.usage = full_usage;
        }

        const GpuId id = slot.buffer.get();
        if (data != nullptr && size > 0U)
        {
            if (auto result = backend->write_buffer(id, data, size, 0U); !result)
                return INVALID_GPU_ID;
        }
        return id;
    }
}
