#include "tbx/systems/graphics/pipeline/render_pass_operation.h"
#include "tbx/systems/graphics/pipeline/render_command_executor.h"
#include <any>
#include <optional>
#include <utility>

namespace tbx
{
    static bool is_cancelled(const CancellationToken& cancellation_token)
    {
        return cancellation_token && cancellation_token.is_cancelled();
    }

    GraphicsRenderPassOperation::GraphicsRenderPassOperation(GraphicsRenderPass pass)
        : _pass(std::move(pass))
    {
    }

    Result GraphicsRenderPassOperation::execute(
        const std::any& payload,
        const CancellationToken& cancellation_token)
    {
        if (is_cancelled(cancellation_token))
            return Result(false, "Graphics render pass operation cancelled.");

        std::optional<std::reference_wrapper<const GraphicsPipelinePayload>> graphics_payload =
            std::nullopt;
        try
        {
            graphics_payload = std::cref(std::any_cast<const GraphicsPipelinePayload&>(payload));
        }
        catch (const std::bad_any_cast&)
        {
            return Result(
                false,
                "Graphics render pass operation requires a graphics pipeline payload.");
        }

        auto& backend = graphics_payload->get().backend.get();

        if (_pass.viewport.has_value())
        {
            if (const auto result = backend.set_viewport(_pass.viewport.value()); !result)
                return result;
        }

        if (const auto result = backend.begin_pass(_pass.pass); !result)
            return result;

        const auto executor = RenderCommandExecutor();
        for (const auto& draw : _pass.draws)
        {
            if (is_cancelled(cancellation_token))
                return Result(false, "Graphics render pass operation cancelled.");

            if (const auto result = executor.execute_draw(backend, draw); !result)
                return result;
        }

        for (const auto& draw : _pass.indexed_draws)
        {
            if (is_cancelled(cancellation_token))
                return Result(false, "Graphics render pass operation cancelled.");

            if (const auto result = executor.execute_indexed_draw(backend, draw); !result)
                return result;
        }

        return backend.end_pass();
    }

    const GraphicsRenderPass& GraphicsRenderPassOperation::get_pass() const
    {
        return _pass;
    }
}
