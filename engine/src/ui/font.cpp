#include "tbx/ui/font.h"
#include "tbx/files/files.h"

namespace tbx
{
    Result<Font> deserialize_font(const std::filesystem::path& path)
    {
        auto data = read_bytes(path);
        if (!data)
            return std::unexpected(data.error());
        if (data->empty())
            return fail("'{}' is empty — not a font", path.string());
        auto font = Font();
        font.data = std::move(*data);
        return ok(std::move(font));
    }
}
