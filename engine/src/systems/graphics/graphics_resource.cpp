#include "tbx/systems/graphics/graphics_resource.h"
#include "tbx/systems/debugging/macros.h"
#include <utility>

namespace tbx
{
    GraphicsResource::GraphicsResource(
        std::weak_ptr<IGraphicsBackend> backend,
        const Uuid resource_uuid)
        : _backend(std::move(backend))
        , _resource_uuid(resource_uuid)
    {
    }

    GraphicsResource::~GraphicsResource() noexcept
    {
    }

    Uuid GraphicsResource::get_uuid() const
    {
        return _resource_uuid;
    }

    void GraphicsResource::update(const std::any&)
    {
        TBX_TRACE_WARNING_ONCE(
            "Graphics resource ({}) does not support updates.",
            to_string(get_uuid()));
    }

    std::shared_ptr<IGraphicsBackend> GraphicsResource::lock_backend() const
    {
        return _backend.lock();
    }

    void GraphicsResource::set_uuid(const Uuid resource_uuid)
    {
        _resource_uuid = resource_uuid;
    }
}
