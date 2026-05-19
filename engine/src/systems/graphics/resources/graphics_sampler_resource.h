#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/graphics_resource.h"
#include <memory>

namespace tbx::detail
{
    class GraphicsSamplerResource final : public GraphicsResource
    {
      public:
        GraphicsSamplerResource(std::weak_ptr<IGraphicsBackend> backend, GraphicsSamplerDesc desc);
        ~GraphicsSamplerResource() noexcept override;

      private:
        bool create_resource();

      private:
        GraphicsSamplerDesc _desc = {};
    };
}
