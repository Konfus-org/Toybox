#include "gpu_resources.h"
#include <utility>

namespace tbx
{
    GpuResource::GpuResource(std::weak_ptr<IGraphicsBackend> backend, GpuId id)
        : _backend(std::move(backend))
        , _id(id)
    {
    }

    GpuResource::~GpuResource()
    {
        reset();
    }

    GpuResource::GpuResource(GpuResource&& other) noexcept
        : _backend(std::move(other._backend))
        , _id(other._id)
    {
        other._id = INVALID_GPU_ID;
    }

    GpuResource& GpuResource::operator=(GpuResource&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            _backend = std::move(other._backend);
            _id = other._id;
            other._id = INVALID_GPU_ID;
        }
        return *this;
    }

    GpuId GpuResource::get() const
    {
        return _id;
    }

    bool GpuResource::is_valid() const
    {
        return _id != INVALID_GPU_ID;
    }

    void GpuResource::reset()
    {
        if (_id == INVALID_GPU_ID)
            return;
        if (const auto backend = _backend.lock())
            backend->destroy_resource(_id);
        _id = INVALID_GPU_ID;
    }
}
