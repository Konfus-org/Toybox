#include "bridge_utils.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/components/renderer.h"
#include "tbx/types/matrices.h"
#include <cctype>
#include <charconv>

namespace tbx::studio_bridge
{
    Result require_object(const tbx::Json& params)
    {
        return params.is_object() ? Result::OK : Result(false, "Missing request parameters.");
    }

    Result require_uint(const tbx::Json& params, std::string_view key, uint64& out)
    {
        const auto iterator = params.find(key);
        if (iterator == params.end() || !iterator->is_number_unsigned())
            return Result(false, "Missing or invalid '" + std::string(key) + "'.");

        out = iterator->get<uint64>();
        return Result::OK;
    }

    Result require_string(const tbx::Json& params, std::string_view key, std::string& out)
    {
        out = params.value(key, std::string());
        return out.empty() ? Result(false, "Missing '" + std::string(key) + "'.") : Result::OK;
    }

    std::string asset_type_from_path(const std::filesystem::path& path)
    {
        auto extension = path.extension().string();
        if (!extension.empty() && extension.front() == '.')
            extension.erase(extension.begin());
        if (extension.empty())
            return "asset";

        for (auto& character : extension)
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

        return extension;
    }

    bool compute_world_focus(tbx::World& world, glm::vec3& out_focus)
    {
        auto sum = glm::vec3(0.0F);
        auto count = 0;
        for (auto& entity : world.get_with<tbx::Renderer>())
        {
            if (!entity.has_component<tbx::Transform>())
                continue;

            sum += entity.get_component<tbx::Transform>().to_world_space(entity).position;
            ++count;
        }

        if (count == 0)
            return false;

        out_focus = sum / static_cast<float>(count);
        return true;
    }

    bool accumulate_entity_world_bounds(
        tbx::AssetManager& assets,
        tbx::Entity entity,
        glm::vec3& out_min,
        glm::vec3& out_max)
    {
        if (!entity.has_component<tbx::Renderer>() || !entity.has_component<tbx::Transform>())
            return false;

        const auto model = assets.load<tbx::Model>(entity.get_component<tbx::Renderer>().model);
        if (!model || model->meshes.empty())
            return false;

        const auto world_matrix = tbx::build_transform_matrix(
            entity.get_component<tbx::Transform>().to_world_space(entity));
        return tbx::expand_aabb_with_model(*model, world_matrix, out_min, out_max);
    }

    bool is_self_or_descendant(tbx::World& world, tbx::Entity entity, const tbx::Uuid& ancestor)
    {
        auto current = entity;
        // The depth guard is a cheap safeguard against a malformed (cyclic) parent chain.
        for (auto guard = 0; current.get_id().is_valid() && guard < 4096; ++guard)
        {
            if (current.get_id().value == ancestor.value)
                return true;

            const auto parent_id = current.get_parent();
            if (!parent_id.is_valid())
                break;
            current = world.get(parent_id);
        }

        return false;
    }

    // Parses the decimal segment following `marker` in a slash-delimited address; zero on no match.
    static uint64 parse_segment_after(std::string_view address, std::string_view marker)
    {
        const auto at = address.find(marker);
        if (at == std::string_view::npos)
            return 0U;

        const auto start = at + marker.size();
        const auto end = address.find('/', start);
        const auto segment =
            address.substr(start, end == std::string_view::npos ? address.size() - start : end - start);

        auto id = uint64(0);
        const auto [ptr, ec] = std::from_chars(segment.data(), segment.data() + segment.size(), id);
        return ec == std::errc() && ptr == segment.data() + segment.size() ? id : 0U;
    }

    uint64 parse_address_entity(std::string_view address)
    {
        if (address.starts_with("entity/"))
            return parse_segment_after(address, "entity/");
        return parse_segment_after(address, "/entities/");
    }

    tbx::Json to_wire_vec3(const tbx::Vec3& value)
    {
        return tbx::Json::array({value.x, value.y, value.z});
    }

    tbx::Json to_wire_quat(const tbx::Quat& value)
    {
        return tbx::Json::array({value.x, value.y, value.z, value.w});
    }
}
