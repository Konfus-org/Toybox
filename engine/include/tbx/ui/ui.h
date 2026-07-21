#pragma once
#include "tbx/core/api.h"
#include "tbx/core/result.h"
#include "tbx/core/typedefs.h"
#include <string>

// The concrete UI boundary (see cmake/tbx_backend.cmake): ui/rmlui/ implements it and its
// library types never escape that folder. State lives in the backend's cpp; initialization is
// lazy on first use. Documents are RML/RCSS text; game UI reaches the screen through the Ui
// block on a toy (the renderer's ui pass loads/shows those documents).
namespace tbx::ui
{
    /// @brief
    /// Purpose: Loads a UI document from RML text and shows it; returns its id for
    /// unload_document/set_document_visible.
    TBX_API Result<uint64> load_document(const std::string& document);

    /// @brief
    /// Purpose: Renders every visible document on top of the frame — called by the renderer's
    /// ui pass.
    TBX_API void render();

    /// @brief
    /// Purpose: Tears the UI down; the next call starts fresh. run() calls this at shutdown.
    TBX_API void reset();

    /// @brief
    /// Purpose: Shows or hides one document by id.
    TBX_API void set_document_visible(uint64 document_id, bool is_visible);

    /// @brief
    /// Purpose: Replaces an element's inline style; the element is found by id across every
    /// loaded document — the minimal dynamic-HUD hook.
    TBX_API void set_inline_style(const std::string& element_id, const std::string& style);

    /// @brief
    /// Purpose: Replaces an element's inner text; the element is found by id across every
    /// loaded document.
    TBX_API void set_text(const std::string& element_id, const std::string& text);

    /// @brief
    /// Purpose: Closes one document by the id load_document returned.
    TBX_API void unload_document(uint64 document_id);

    /// @brief
    /// Purpose: Advances animations/layout; called by tbx::run() every frame.
    TBX_API void update(float delta_time);
}
