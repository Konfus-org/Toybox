#include "graphics_sampler_resource.h"
#include "tbx/systems/debugging/macros.h"
#include <utility>

namespace tbx::detail
{
    GraphicsSamplerResource::GraphicsSamplerResource(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsSamplerDesc desc)
        : GraphicsResource(std::move(backend))
        , _desc(std::move(desc))
    {
        if (!create_resource())
            TBX_TRACE_ERROR_ONCE(
                "Graphics sampler resource ({}) failed to create resource.",
                to_string(get_uuid()));
    }

    bool GraphicsSamplerResource::create_resource()
    {
        const auto backend = lock_backend();
        if (!backend)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics sampler resource ({}) failed to create: graphics backend is unavailable.",
                to_string(get_uuid()));
            return false;
        }

        auto resource_uuid = Uuid {};
        const Result result = backend->upload_sampler(_desc, resource_uuid);
        if (result)
        {
            set_uuid(resource_uuid);
            return true;
        }

        TBX_TRACE_ERROR_ONCE(
            "Graphics sampler resource ({}) create failed: {}",
            to_string(resource_uuid),
            result.get_report());
        return false;
    }

    GraphicsSamplerResource::~GraphicsSamplerResource() noexcept
    {
        if (!get_uuid().is_valid())
            return;

        const auto backend = lock_backend();
        if (!backend)
        {
            TBX_TRACE_WARNING_ONCE(
                "Graphics sampler resource ({}) failed to unload: graphics backend is unavailable.",
                to_string(get_uuid()));
            return;
        }

        const Result result = backend->unload(get_uuid());
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics sampler resource ({}) failed to unload: {}",
                to_string(get_uuid()),
                result.get_report());
        }
    }
}

