#include "frame_buffers.h"
#include <algorithm>

namespace tbx
{
    FrameBuffers::FrameBuffers(std::weak_ptr<IGraphicsBackend> backend)
        : _backend(std::move(backend))
    {
    }

    GpuId FrameBuffers::store(const void* data, const uint64 size, const BufferUsage usage)
    {
        const auto backend = _backend.lock();
        if (!backend)
            return INVALID_GPU_ID;

        auto desc = BufferDesc {
            .usage = usage | BufferUsage::COPY_DST,
            .size = std::max<uint64>(size, 1U),
            .is_dynamic = true};
        auto id = INVALID_GPU_ID;
        if (auto result = backend->create_buffer(desc, id); !result)
            return INVALID_GPU_ID;

        _buffers.emplace_back(_backend, id);
        if (data != nullptr && size > 0U)
        {
            if (auto result = backend->write_buffer(id, data, size, 0U); !result)
                return INVALID_GPU_ID;
        }
        return id;
    }
}
