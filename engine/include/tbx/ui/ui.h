#pragma once
#include "tbx/core/result.h"
#include "tbx/core/typedefs.h"
#include <string>

// The concrete UI boundary (see cmake/tbx_backend.cmake): ui/rmlui/ implements it and its
// library types never escape that folder. State lives in the backend's cpp; initialization is
// lazy on first use. Documents are RML/RCSS text.
namespace tbx::ui
{
    /// @brief
    /// Purpose: Loads a UI document from RML text and shows it; returns its id for unloading.
    Result<uint64> load_document(const std::string& document);

    /// @brief
    /// Purpose: Renders every visible document on top of the frame; call after 3D drawing,
    /// before tbx::run() presents.
    void render();

    /// @brief
    /// Purpose: Tears the UI down; the next call starts fresh. run() calls this at shutdown.
    void reset();

    /// @brief
    /// Purpose: Replaces an element's inline style by its id — the minimal dynamic-HUD hook.
    void set_inline_style(
        uint64 document_id,
        const std::string& element_id,
        const std::string& style);

    /// @brief
    /// Purpose: Closes one document by the id load_document returned.
    void unload_document(uint64 document_id);

    /// @brief
    /// Purpose: Advances animations/layout; called by tbx::run() every frame.
    void update(float delta_time);
}
