#include "tbx/gfx/shader_source.h"
#include "tbx/files/files.h"

namespace tbx::assets
{
    template <>
    Result<gfx::ShaderSource> load<gfx::ShaderSource>(const std::filesystem::path& path)
    {
        auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        auto source = gfx::ShaderSource();
        source.text = std::move(*text);
        return ok(std::move(source));
    }
}
