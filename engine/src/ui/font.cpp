#include "tbx/ui/font.h"
#include "tbx/files/files.h"

namespace tbx::assets
{
    template <>
    Result<ui::Font> load<ui::Font>(const std::filesystem::path& path)
    {
        auto data = files::read_bytes(path);
        if (!data)
            return std::unexpected(data.error());
        if (data->empty())
            return fail("'{}' is empty — not a font", path.string());
        auto font = ui::Font();
        font.data = std::move(*data);
        return ok(std::move(font));
    }
}
