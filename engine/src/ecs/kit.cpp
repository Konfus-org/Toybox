#include "tbx/ecs/kit.h"
#include "tbx/serialization/json.h"

namespace tbx
{
    template <>
    Result<Kit> load<Kit>(const std::filesystem::path& path)
    {
        auto body = load<Json>(path);
        if (!body)
            return std::unexpected(body.error());
        if (!body->is_object())
            return fail("'{}' is not a kit (JSON object expected)", path.string());
        return Kit {.body = std::move(*body)};
    }
}
