#include "tbx/ui/font.h"
#include "tbx/files/files.h"

namespace tbx
{
    template <>
    Result<Font> load<Font>(const std::filesystem::path& path)
    {
        auto data = files::read_bytes(path);
        if (!data)
            return std::unexpected(data.error());
        if (data->empty())
            return fail("'{}' is empty — not a font", path.string());
        return Font {.data = std::move(*data)};
    }
}
