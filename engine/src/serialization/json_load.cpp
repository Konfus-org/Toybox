#include "tbx/serialization/json.h"
#include "tbx/files/files.h"

namespace tbx
{
    template <>
    Result<Json> load<Json>(const std::filesystem::path& path)
    {
        auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        if (!is_valid_json(*text))
            return fail("'{}' is not valid JSON", path.string());
        return parse_json(*text);
    }
}
