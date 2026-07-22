#include "tbx/scripting/script_source.h"
#include "tbx/files/files.h"

namespace tbx::assets
{
    template <>
    Result<ScriptSource> load<ScriptSource>(const std::filesystem::path& path)
    {
        auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        auto source = ScriptSource();
        source.name = path.filename().string();
        source.source = std::move(*text);
        return ok(std::move(source));
    }
}
