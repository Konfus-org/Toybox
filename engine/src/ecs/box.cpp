#include "tbx/ecs/box.h"
#include "tbx/serialization/json.h"

namespace tbx
{
    template <>
    Result<Box> load<Box>(const std::filesystem::path& path)
    {
        auto body = load<Json>(path);
        if (!body)
            return std::unexpected(body.error());
        if (!body->is_object())
            return fail("'{}' is not a box (JSON object expected)", path.string());

        auto box = Box {};
        try
        {
            for (const Json& entry : body->value("kits", Json::array()))
            {
                auto box_entry = BoxEntry {};
                box_entry.kit = AssetHandle<Kit>(entry.value("reference", std::string()));
                if (entry.value("mode", std::string("always")) == "streamed")
                    box_entry.mode = KitMode::STREAMED;
                if (entry.contains("position"))
                    box_entry.position = Vec3(
                        entry["position"].at(0).get<float>(),
                        entry["position"].at(1).get<float>(),
                        entry["position"].at(2).get<float>());
                box.kits.push_back(std::move(box_entry));
            }
        }
        catch (const Json::exception& e)
        {
            return fail("malformed box '{}': {}", path.string(), e.what());
        }
        return box;
    }
}
