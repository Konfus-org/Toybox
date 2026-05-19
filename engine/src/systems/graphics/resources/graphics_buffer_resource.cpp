#include "graphics_buffer_resource.h"
#include "tbx/systems/debugging/macros.h"
#include <utility>

namespace tbx::detail
{
    GraphicsBufferResource::GraphicsBufferResource(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsBufferDesc desc,
        std::vector<uint8> upload_data)
        : GraphicsResource(std::move(backend))
        , _desc(std::move(desc))
        , _upload_data(std::move(upload_data))
    {
        if (!create_resource())
            TBX_TRACE_ERROR_ONCE(
                "Graphics buffer resource ({}) failed to create resource.",
                to_string(get_uuid()));
    }

    void GraphicsBufferResource::update(const std::any& data)
    {
        const auto request = std::any_cast<GraphicsBufferUpdateRequest>(&data);
        if (request == nullptr)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics buffer resource ({}) failed to apply invalid update payload.",
                to_string(get_uuid()));
            return;
        }

        const auto backend = lock_backend();
        if (!backend)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics buffer resource ({}) failed: graphics backend is unavailable.",
                to_string(get_uuid()));
            return;
        }

        const Result result = backend->update_buffer(
            get_uuid(),
            request->data,
            request->data_size,
            request->offset);
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics buffer resource ({}) update failed: {}",
                to_string(get_uuid()),
                result.get_report());
        }
    }

    GraphicsBufferResource::~GraphicsBufferResource() noexcept
    {
        if (!get_uuid().is_valid())
            return;

        const auto backend = lock_backend();
        if (!backend)
        {
            TBX_TRACE_WARNING_ONCE(
                "Graphics buffer resource ({}) failed to unload: graphics backend is unavailable.",
                to_string(get_uuid()));
            return;
        }

        const Result result = backend->unload(get_uuid());
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics buffer resource ({}) failed to unload: {}",
                to_string(get_uuid()),
                result.get_report());
        }
    }

    bool GraphicsBufferResource::create_resource()
    {
        const auto backend = lock_backend();
        if (!backend)
        {
            TBX_TRACE_ERROR_ONCE(
                "Graphics buffer resource ({}) failed to create: graphics backend is unavailable.",
                to_string(get_uuid()));
            return false;
        }

        const void* upload_data = _upload_data.empty() ? nullptr : _upload_data.data();
        auto resource_uuid = Uuid {};
        const Result result = backend->upload_buffer(
            _desc,
            upload_data,
            static_cast<uint64>(_upload_data.size()),
            resource_uuid);
        if (result)
        {
            set_uuid(resource_uuid);
            return true;
        }

        TBX_TRACE_ERROR_ONCE(
            "Graphics buffer resource ({}) create failed: {}",
            to_string(resource_uuid),
            result.get_report());
        return false;
    }
}

