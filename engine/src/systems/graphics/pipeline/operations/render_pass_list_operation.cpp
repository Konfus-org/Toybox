#include "tbx/systems/graphics/pipeline/operations/render_pass_list_operation.h"

namespace tbx
{
RenderPassListOperation::RenderPassListOperation(std::string debug_name, std::string category, PassListSelector pass_list_selector)
: _debug_info({.debug_name=std::move(debug_name), .category=std::move(category)}), _pass_list_selector(pass_list_selector) {}
RenderOperationDebugInfo RenderPassListOperation::get_debug_info() const { return _debug_info; }
Result RenderPassListOperation::prepare(FrameData&) { return {}; }
Result RenderPassListOperation::execute(IGraphicsBackend& backend, FrameData& frame_data, const CancellationToken& token)
{
    if (_pass_list_selector == nullptr) return Result(false, "Render pass operation has no pass list selector.");
    for (const auto& render_pass : frame_data.*_pass_list_selector)
    {
        // Inline execution for readability: begin pass, bind resources, issue draws, end pass.
        if (token.is_cancelled()) return Result(false, "Render pass list cancelled.");
        auto result = backend.begin_pass(render_pass.pass);
        if (!result) return result;
        if (render_pass.viewport.has_value()) { result = backend.set_viewport(render_pass.viewport.value()); if (!result) return result; }
        for (const auto& command : render_pass.draws)
        {
            if (token.is_cancelled()) return Result(false, "Render command execution cancelled while drawing.");
            result = backend.bind_pipeline(command.pipeline); if (!result) return result;
            for (const auto& b : command.vertex_buffers) { result = backend.bind_vertex_buffer(b.slot, b.resource); if (!result) return result; }
            for (const auto& b : command.uniform_buffers) { result = backend.bind_uniform_buffer(b.slot, b.resource); if (!result) return result; }
            for (const auto& b : command.storage_buffers) { result = backend.bind_storage_buffer(b.slot, b.resource); if (!result) return result; }
            for (const auto& b : command.textures) { result = backend.bind_texture(b.slot, b.resource); if (!result) return result; }
            for (const auto& b : command.samplers) { result = backend.bind_sampler(b.slot, b.resource); if (!result) return result; }
            result = backend.draw(command.vertex_count, command.vertex_offset); if (!result) return result;
        }
        for (const auto& command : render_pass.indexed_draws)
        {
            if (token.is_cancelled()) return Result(false, "Render command execution cancelled while drawing indexed.");
            result = backend.bind_pipeline(command.pipeline); if (!result) return result;
            for (const auto& b : command.vertex_buffers) { result = backend.bind_vertex_buffer(b.slot, b.resource); if (!result) return result; }
            result = backend.bind_index_buffer(command.index_buffer, command.index_type); if (!result) return result;
            for (const auto& b : command.uniform_buffers) { result = backend.bind_uniform_buffer(b.slot, b.resource); if (!result) return result; }
            for (const auto& b : command.storage_buffers) { result = backend.bind_storage_buffer(b.slot, b.resource); if (!result) return result; }
            for (const auto& b : command.textures) { result = backend.bind_texture(b.slot, b.resource); if (!result) return result; }
            for (const auto& b : command.samplers) { result = backend.bind_sampler(b.slot, b.resource); if (!result) return result; }
            result = backend.draw_indexed(command.draw); if (!result) return result;
        }
        result = backend.end_pass(); if (!result) return result;
    }
    return {};
}
}
