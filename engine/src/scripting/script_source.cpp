#include "tbx/scripting/source.h"
#include "tbx/files/files.h"

namespace tbx::scripts
{
    Result<Source> deserialize_script_source(const std::filesystem::path& path)
    {
        auto text = read_text(path);
        if (!text)
            return std::unexpected(text.error());
        auto source = Source();
        source.name = path.filename().string();
        source.source = std::move(*text);
        return ok(std::move(source));
    }
}
