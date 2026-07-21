#include "tbx/gfx/material.h"
#include "tbx/files/files.h"

namespace tbx
{
    template <>
    Result<Material> load<Material>(const std::filesystem::path& path)
    {
        auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        if (!serialization::is_valid(*text))
            return fail("'{}' is not a valid material", path.string());
        const serialization::Json data = serialization::parse(*text);

        // References stay authoring-time paths (or uuids) on the handles; the asset system
        // resolves them on first load.
        auto material = Material {};
        const auto read_reference = [&]<typename TAsset>(const char* key, AssetHandle<TAsset>& into)
        {
            if (!data.contains(key))
                return;
            auto text = data[key].get<std::string>();
            auto stripped = text;
            std::erase(stripped, '-');
            const Uuid id = Uuid::parse(stripped);
            if (id.is_nil())
                into.path = std::move(text);
            else
                into.id = id;
        };
        read_reference("vertex", material.vertex);
        read_reference("fragment", material.fragment);
        read_reference("albedo_map", material.albedo_map);
        read_reference("normal_map", material.normal_map);
        read_reference("metallic_roughness_map", material.metallic_roughness_map);

        const auto read_color = [&](const char* key, Color& into)
        {
            if (!data.contains(key) || !data[key].is_array() || data[key].size() < 3)
                return;
            into.r = data[key][0].get<float>();
            into.g = data[key][1].get<float>();
            into.b = data[key][2].get<float>();
            into.a = data[key].size() > 3 ? data[key][3].get<float>() : 1.0f;
        };
        read_color("albedo", material.albedo);
        read_color("emissive", material.emissive);
        material.metallic = data.value("metallic", material.metallic);
        material.roughness = data.value("roughness", material.roughness);
        if (data.contains("uniforms") && data["uniforms"].is_object())
            material.uniforms = data["uniforms"];
        return material;
    }
}
