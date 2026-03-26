#pragma once
#include "tbx/common/int.h"
#include "tbx/graphics/messages.h"

namespace opengl_rendering
{
    /// <summary>Sets the graphics API proc-address loader used by bindless helpers.</summary>
    /// <remarks>Purpose: Reuses the active context's loader instead of platform-specific symbols.
    /// Ownership: Stores a non-owning function pointer.
    /// Thread Safety: Call during renderer initialization on the render thread.</remarks>
    void set_bindless_proc_loader(tbx::GraphicsProcAddress loader);

    /// <summary>Queries whether ARB bindless texture calls are available.</summary>
    /// <remarks>Purpose: Guards optional bindless texture usage at runtime.
    /// Ownership: Stateless utility; no ownership transfer.
    /// Thread Safety: Safe to call on the render thread after GL initialization.</remarks>
    bool is_bindless_texture_supported();

    /// <summary>Creates and makes a bindless handle resident for a texture.</summary>
    /// <remarks>Purpose: Exposes a texture through a 64-bit bindless handle.
    /// Ownership: The caller owns texture lifetime; handle residency is tied to that lifetime.
    /// Thread Safety: Render thread only.</remarks>
    bool try_make_bindless_handle_resident(tbx::uint32 texture_id, tbx::uint64& out_handle);

    /// <summary>Releases residency for a bindless texture handle.</summary>
    /// <remarks>Purpose: Drops bindless residency before destroying texture resources.
    /// Ownership: Does not destroy the texture object itself.
    /// Thread Safety: Render thread only.</remarks>
    void release_bindless_handle(tbx::uint64 handle);

    /// <summary>Uploads a bindless texture handle into a sampler uniform.</summary>
    /// <remarks>Purpose: Binds a texture to a shader sampler without texture units.
    /// Ownership: Does not transfer ownership of shader or texture resources.
    /// Thread Safety: Render thread only.</remarks>
    bool try_upload_bindless_sampler(
        tbx::uint32 program_id,
        int uniform_location,
        tbx::uint64 handle);
}
