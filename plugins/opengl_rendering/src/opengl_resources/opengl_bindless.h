#pragma once
#include "tbx/common/typedefs.h"
#include "tbx/graphics/opengl_context_manager.h"

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Reuses the active context's loader instead of platform-specific symbols.
    /// @details
    /// Ownership: Stores a non-owning function pointer.
    /// Thread Safety: Call during renderer initialization on the render thread.
    void set_bindless_proc_loader(tbx::GraphicsProcAddress loader);

    /// @brief
    /// Purpose: Guards optional bindless texture usage at runtime.
    /// @details
    /// Ownership: Stateless utility; no ownership transfer.
    /// Thread Safety: Safe to call on the render thread after GL initialization.
    bool is_bindless_texture_supported();

    /// @brief
    /// Purpose: Exposes a texture through a 64-bit bindless handle.
    /// @details
    /// Ownership: The caller owns texture lifetime; handle residency is tied to that lifetime.
    /// Thread Safety: Render thread only.
    bool try_make_bindless_handle_resident(uint32 texture_id, uint64& out_handle);

    /// @brief
    /// Purpose: Drops bindless residency before destroying texture resources.
    /// @details
    /// Ownership: Does not destroy the texture object itself.
    /// Thread Safety: Render thread only.
    void release_bindless_handle(uint64 handle);

    /// @brief
    /// Purpose: Binds a texture to a shader sampler without texture units.
    /// @details
    /// Ownership: Does not transfer ownership of shader or texture resources.
    /// Thread Safety: Render thread only.
    bool try_upload_bindless_sampler(
        uint32 program_id,
        int uniform_location,
        uint64 handle);
}
