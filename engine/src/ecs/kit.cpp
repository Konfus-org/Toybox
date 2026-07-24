#include "tbx/ecs/kit.h"
#include "tbx/files/files.h"
#include "tbx/math/transform.h"
#include "ecs_internal.h" // internal::bounds_to_json — shared with sandbox serialization
#include <filesystem>

namespace tbx
{

    namespace internal
    {
        /// @brief
        /// Purpose: Back-compat: an older .kit stored nested kits in a separate "kits" array of
        /// {reference, position/rotation/scale}. Read each as a toy wearing a KitInstance block
        /// positioned by its transform — the same shape serialize_kit now emits. Files migrate to the
        /// new form on their next save.
        static void read_legacy_references(Kit& kit, const Json& body)
        {
            for (const Json& entry : body.value("kits", Json::array()))
            {
                Toy toy = kit.add("KitReference");
                auto& transform = toy.get_transform();
                if (entry.contains("position"))
                    transform.position = Vec3(
                        entry["position"].at(0).get<float>(),
                        entry["position"].at(1).get<float>(),
                        entry["position"].at(2).get<float>());
                if (entry.contains("rotation"))
                    transform.rotation = Quat(
                        entry["rotation"].at(3).get<float>(), // w (stored [x, y, z, w])
                        entry["rotation"].at(0).get<float>(),
                        entry["rotation"].at(1).get<float>(),
                        entry["rotation"].at(2).get<float>());
                if (entry.contains("scale"))
                    transform.scale = Vec3(
                        entry["scale"].at(0).get<float>(),
                        entry["scale"].at(1).get<float>(),
                        entry["scale"].at(2).get<float>());
                auto text = entry.value("reference", std::string());
                auto stripped = text;
                std::erase(stripped, '-');
                const Uuid uuid = Uuid::parse(stripped);
                toy.with(
                    KitInstance {
                        .kit = uuid.is_valid() ? AssetHandle<Kit>(uuid)
                                               : AssetHandle<Kit>(std::move(text))});
            }
        }

    }
    //// JSON BOUNDARY ////


    //// DISK BOUNDARY (the registered serializer functions) ////

    Result<Kit> deserialize_kit(const std::filesystem::path& path)
    {
        const auto text = read_text(path);
        if (!text)
            return std::unexpected(text.error());
        if (!Json::accept(*text))
            return fail("'{}' is not a kit (JSON expected)", path.string());
        const auto body = Json::parse(*text, nullptr, false);
        if (!body.is_object())
            return fail("'{}' kit body is not a JSON object", path.string());

        auto kit = Kit();
        try
        {
            if (auto toys = deserialize_toys(kit, body.value("toys", Json::array())); !toys)
                return std::unexpected(toys.error());
            internal::read_legacy_references(kit, body); // older .kit "kits" array, if any
            const auto bounds = body.value("bounds", Json::object());
            if (bounds.contains("center"))
                kit.bounds_center = Vec3(
                    bounds["center"].at(0).get<float>(),
                    bounds["center"].at(1).get<float>(),
                    bounds["center"].at(2).get<float>());
            kit.bounds_radius = bounds.value("radius", 0.0f);
        }
        catch (const Json::exception& e)
        {
            return fail("malformed kit body: {}", e.what());
        }
        return ok(std::move(kit));
    }

    Result<void> serialize_kit(const Kit& kit, const std::filesystem::path& path)
    {
        auto body = Json::object();
        body["toys"] = serialize_toys(kit);
        body["bounds"] = internal::bounds_to_json(kit.bounds_center, kit.bounds_radius);
        return write_text(path.string(), body.dump(4));
    }
}
