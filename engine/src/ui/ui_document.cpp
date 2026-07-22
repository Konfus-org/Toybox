#include "tbx/ui/ui_document.h"
#include "tbx/files/files.h"

namespace tbx
{
    template <>
    Result<UiDocument> load<UiDocument>(const std::filesystem::path& path)
    {
        // RML is just text; nothing library-specific happens until the document draws.
        auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        auto document = UiDocument();
        document.source = std::move(*text);
        return ok(std::move(document));
    }
}
