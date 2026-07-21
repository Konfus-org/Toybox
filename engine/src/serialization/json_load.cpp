#include "tbx/serialization/json.h"
#include "tbx/files/files.h"

namespace tbx
{
    template <>
    Result<serialization::Json> load<serialization::Json>(const std::filesystem::path& path)
    {
        auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        if (!serialization::is_valid(*text))
            return fail("'{}' is not valid JSON", path.string());
        return serialization::parse(*text);
    }
}
