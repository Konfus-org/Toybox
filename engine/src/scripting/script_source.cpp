#include "tbx/scripting/script_source.h"
#include "tbx/files/files.h"

namespace tbx
{
    template <>
    Result<ScriptSource> load<ScriptSource>(const std::filesystem::path& path)
    {
        auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        return ScriptSource {.name = path.filename().string(), .source = std::move(*text)};
    }
}
