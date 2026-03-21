#pragma once
#include "OpenGlFrameContext.h"
#include "opengl_resources/opengl_resource_manager.h"
#include <any>

namespace opengl_rendering
{
    /// <summary>
    /// Purpose: Placeholder shadow pass that is temporarily disabled.
    /// </summary>
    /// <remarks>
    /// Ownership: Does not retain render resources while disabled.
    /// Thread Safety: Not thread-safe; render-thread only.
    /// </remarks>
    class ShadowPassOperation final
    {
      public:
        ShadowPassOperation(OpenGlResourceManager& resource_manager);
        ShadowPassOperation(const ShadowPassOperation&) = delete;
        ShadowPassOperation& operator=(const ShadowPassOperation&) = delete;
        ~ShadowPassOperation() noexcept;

        /// <summary>
        /// Purpose: Placeholder execute endpoint while shadow rendering is disabled.
        /// Ownership: Does not take ownership of the supplied payload.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        void execute(const std::any& payload);

        /// <summary>
        /// Purpose: Returns no texture while shadow rendering is disabled.
        /// Ownership: Returns a non-owning null texture handle.
        /// Thread Safety: Not thread-safe; render-thread only.
        /// </summary>
        tbx::uint32 get_shadow_texture() const;
    };
}
