#include "glass_ops.h"
#include "engine_services.h"
#include "glass_state.h"
#include "wire.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/types/components/post_processing.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/vectors.h"
#include <cstring>
#include <format>
#include <memory>
#include <utility>
#include <vector>

namespace tbx::studio_bridge
{
    // The shader's parameter-lane budget: lane 0 is the meta lane, lanes 1..7 one rect each.
    static constexpr size GLASS_MAX_RECTS = 7U;

    // The default blur reach in pixels — soft enough to read as frost, small enough to stay sharp
    // at the card's edge.
    static constexpr float GLASS_DEFAULT_RADIUS = 7.0F;

    // Order-sensitive FNV-1a over the rect floats, so an unchanged push skips the pass rebuild.
    static uint64 hash_rects(const std::vector<tbx::Vec4>& rects, float radius)
    {
        auto hash = 1469598103934665603ULL;
        const auto mix = [&hash](float value)
        {
            uint32 bits = 0U;
            std::memcpy(&bits, &value, sizeof(bits));
            hash ^= bits;
            hash *= 1099511628211ULL;
        };
        mix(radius);
        for (const auto& rect : rects)
        {
            mix(rect.x);
            mix(rect.y);
            mix(rect.z);
            mix(rect.w);
        }
        return hash;
    }

    Result set_view_glass(GlassState& state, const EngineServices& services, const tbx::Json& params)
    {
        const auto view = params.value(Wire::VIEW, std::string());
        if (view.empty())
            return Result(false, "view.setGlass needs a 'view'.");

        auto rects = std::vector<tbx::Vec4>();
        if (const auto list = params.find(Wire::RECTS); list != params.end() && list->is_array())
            for (const auto& entry : *list)
            {
                if (rects.size() >= GLASS_MAX_RECTS)
                    break;
                if (entry.is_array() && entry.size() >= 4U)
                    rects.emplace_back(
                        entry[0].get<float>(),
                        entry[1].get<float>(),
                        entry[2].get<float>(),
                        entry[3].get<float>());
            }
        const auto radius = params.value("radius", GLASS_DEFAULT_RADIUS);

        auto& entry = state.views[view];
        const auto hash = hash_rects(rects, radius);
        if (hash == entry.hash && (entry.pass.is_valid() || rects.empty()))
            return Result::OK;
        entry.hash = hash;

        auto rendering = services.rendering.lock();
        if (!rendering)
            return Result(false, "Rendering service is unavailable.");

        // Replace, never mutate: the render lane reads a registered pass's effects without a lock,
        // so a rect change swaps in a fresh pass (an in-flight frame keeps the old one alive
        // through its shared_ptr).
        if (entry.pass.is_valid())
        {
            rendering->remove_render_pass(entry.pass);
            entry.pass = {};
        }
        if (rects.empty())
            return Result::OK;

        // One Post pass gated on THIS view's name tag (editor cameras carry it), carrying the blur
        // effect whose parameter lanes are the meta lane + the rects, in shader order.
        auto material = tbx::MaterialInstance(tbx::Handle("Materials/Gizmos/GlassBlur.mat"));
        material.set_parameter(
            "meta", tbx::Vec4(static_cast<float>(rects.size()), radius, 0.0F, 0.0F));
        for (size index = 0; index < rects.size(); ++index)
            material.set_parameter(std::format("rect{}", index), rects[index]);

        auto pass = std::make_shared<tbx::RenderPass>(
            tbx::PassType::Post, std::vector<std::string> {view});
        pass->post_effects.push_back(tbx::PostProcessingEffect {
            .material = std::move(material),
            .is_enabled = true,
            .blend = 1.0F,
            .tags = {}});
        entry.pass = rendering->add_render_pass(std::move(pass));
        return Result::OK;
    }

    void clear_view_glass(GlassState& state, const EngineServices& services, const std::string& view)
    {
        const auto entry = state.views.find(view);
        if (entry == state.views.end())
            return;
        if (auto rendering = services.rendering.lock(); rendering && entry->second.pass.is_valid())
            rendering->remove_render_pass(entry->second.pass);
        state.views.erase(entry);
    }

    void clear_all_glass(GlassState& state, const EngineServices& services)
    {
        if (auto rendering = services.rendering.lock())
            for (const auto& [view, entry] : state.views)
                if (entry.pass.is_valid())
                    rendering->remove_render_pass(entry.pass);
        state.views.clear();
    }
}
