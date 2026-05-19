#pragma once
#include "tbx/tbx_api.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/types/uuid.h"
#include <any>
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: Base class shared by all graphics resources.
    /// @details
    /// Ownership: Implementations own their backend configuration and realized GPU identifiers.
    /// Thread Safety: Not inherently thread-safe; call through the graphics resource manager.
    class TBX_API GraphicsResource
    {
      public:
        GraphicsResource(
            std::weak_ptr<IGraphicsBackend> backend,
            Uuid resource_uuid = {});
        virtual ~GraphicsResource() noexcept;

      public:
        /// @brief
        /// Purpose: Returns the stable Toybox identifier exposed for this graphics resource.
        virtual Uuid get_uuid() const;

        /// @brief
        /// Purpose: Applies a backend-specific update payload to the resource.
        virtual void update(const std::any& data);

      protected:
        std::shared_ptr<IGraphicsBackend> lock_backend() const;
        void set_uuid(Uuid resource_uuid);

      private:
        std::weak_ptr<IGraphicsBackend> _backend = {};
        Uuid _resource_uuid = {};
    };
}
