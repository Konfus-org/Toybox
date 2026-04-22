#pragma once
#include "opengl_resource.h"
#include "tbx/graphics/graphics_backend.h"
#include <glad/glad.h>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Owns an OpenGL sampler object for explicit texture sampling.
    /// @details
    /// Ownership: Owns one OpenGL sampler handle.
    /// Thread Safety: Not thread-safe; use on the active OpenGL context thread.
    class OpenGlSampler final : public IOpenGlResource
    {
      public:
        OpenGlSampler(const tbx::GraphicsSamplerDesc& desc);
        ~OpenGlSampler() noexcept override;

      public:
        OpenGlSampler(const OpenGlSampler&) = delete;
        OpenGlSampler(OpenGlSampler&& other) noexcept;
        OpenGlSampler& operator=(const OpenGlSampler&) = delete;
        OpenGlSampler& operator=(OpenGlSampler&& other) noexcept;

      public:
        void bind() override;
        void bind_slot(uint32 slot) const;
        GLuint get_sampler_id() const;
        void unbind() override;

      private:
        GLuint _sampler_id = 0U;
    };
}
