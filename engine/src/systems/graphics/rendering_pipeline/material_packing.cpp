#include "material_packing.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/handle.h"
#include <cstring>
#include <variant>
#include <vector>

namespace tbx
{
    //// STATIC HELPERS ////

    static std::string describe_handle(const Handle& handle)
    {
        if (!handle.name.empty())
            return handle.name;
        return std::string("<id ") + std::to_string(static_cast<uint32>(handle.id)) + ">";
    }

    // Flattens one material parameter value into its float components, in declared order, so the GPU
    // record's params[] is a positional float stream the shader addresses by lane.
    static void append_param_floats(const MaterialParameterData& data, std::vector<float>& out)
    {
        std::visit(
            [&out](const auto& value)
            {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, bool>)
                    out.push_back(value ? 1.0F : 0.0F);
                else if constexpr (std::is_same_v<T, int>)
                    out.push_back(static_cast<float>(value));
                else if constexpr (std::is_same_v<T, float>)
                    out.push_back(value);
                else if constexpr (std::is_same_v<T, double>)
                    out.push_back(static_cast<float>(value));
                else if constexpr (std::is_same_v<T, Vec2>)
                {
                    out.push_back(value.x);
                    out.push_back(value.y);
                }
                else if constexpr (std::is_same_v<T, Vec3>)
                {
                    out.push_back(value.x);
                    out.push_back(value.y);
                    out.push_back(value.z);
                }
                else if constexpr (std::is_same_v<T, Vec4>)
                {
                    out.push_back(value.x);
                    out.push_back(value.y);
                    out.push_back(value.z);
                    out.push_back(value.w);
                }
                else if constexpr (std::is_same_v<T, Color>)
                {
                    out.push_back(value.r);
                    out.push_back(value.g);
                    out.push_back(value.b);
                    out.push_back(value.a);
                }
                else if constexpr (std::is_same_v<T, Mat3>)
                {
                    for (int c = 0; c < 3; ++c)
                        for (int r = 0; r < 3; ++r)
                            out.push_back(value[c][r]);
                }
                else if constexpr (std::is_same_v<T, Mat4>)
                {
                    for (int c = 0; c < 4; ++c)
                        for (int r = 0; r < 4; ++r)
                            out.push_back(value[c][r]);
                }
            },
            data);
    }

    //// PUBLIC ////

    GpuMaterialData pack_material(
        GpuResourceCache& cache,
        const Material& material,
        const std::string& material_name,
        RenderFailure& out_failure)
    {
        out_failure = RenderFailure::NONE;
        auto packed = GpuMaterialData();

        std::vector<float> floats;
        floats.reserve(GPU_MATERIAL_PARAM_FLOAT_COUNT);
        for (const auto& parameter : material.parameters.values)
            append_param_floats(parameter.data, floats);
        if (floats.size() > GPU_MATERIAL_PARAM_FLOAT_COUNT)
        {
            TBX_TRACE_WARNING_ONCE(
                "Material '{}' has more parameter data than the GPU record holds; using yellow "
                "validation.",
                material_name);
            floats.resize(GPU_MATERIAL_PARAM_FLOAT_COUNT);
            out_failure = RenderFailure::INVALID_MATERIAL_DATA;
        }
        if (!floats.empty())
            std::memcpy(packed.params.data(), floats.data(), floats.size() * sizeof(float));

        uint32 slot = 0U;
        for (const auto& binding : material.textures.values)
        {
            if (slot >= GPU_MATERIAL_TEXTURE_SLOT_COUNT)
                break;
            if (!binding.texture.id.is_valid())
            {
                packed.texture_present[slot] = 0U;
                packed.texture_indices[slot] = 0U;
                ++slot;
                continue;
            }

            const auto index = cache.add_texture(
                static_cast<CacheId>(static_cast<uint32>(binding.texture.id)),
                binding.texture,
                false);
            if (!index)
            {
                out_failure = RenderFailure::MISSING_TEXTURE; // takes precedence over data overflow
                TBX_TRACE_WARNING_ONCE(
                    "Texture '{}' for material '{}' failed to load; using cyan checker validation.",
                    describe_handle(binding.texture),
                    material_name);
            }
            packed.texture_indices[slot] = static_cast<uint32>(index.value_or(0U));
            packed.texture_present[slot] = 1U;
            ++slot;
        }

        packed.flags = material.config.blend_mode == MaterialBlendMode::OPAQUE
                           ? GPU_PIPELINE_FLAG_OPAQUE
                           : GPU_PIPELINE_FLAG_TRANSPARENT;
        return packed;
    }

    bool resolve_material_instance(
        AssetManager& assets,
        const MaterialInstance& instance,
        Material& out_material,
        std::string& out_name,
        RenderFailure& out_failure)
    {
        out_failure = RenderFailure::NONE;
        const auto base = assets.load<Material>(instance.material);
        if (!base)
        {
            TBX_TRACE_WARNING_ONCE(
                "Material '{}' failed to load; using red validation.",
                describe_handle(instance.material));
            out_name = "missing_material";
            out_failure = RenderFailure::MISSING_MATERIAL;
            return false;
        }
        out_material = *base;
        out_name = "material_" + std::to_string(static_cast<uint32>(instance.material.id));

        const MaterialOverrides& overrides = instance.overrides;
        if (overrides.has_parameter_override())
            for (const auto& parameter : overrides.parameters)
                out_material.parameters.set(parameter.name, parameter.data);
        if (overrides.has_texture_override())
            for (const auto& texture : overrides.textures)
                out_material.textures.set(texture.name, texture.texture);
        return true;
    }
}
