#pragma once
#include "tbx/common/result.h"
#include "tbx/common/uuid.h"
#include "tbx/messages/dispatcher.h"
#include <functional>

namespace opengl_rendering
{
    using namespace tbx;
    class OpenGlContext final
    {
      public:
        OpenGlContext(IMessageDispatcher& dispatcher, const Uuid& window_id);

        const Uuid& get_window_id() const;
        Result make_current() const;
        Result present() const;

      private:
        std::reference_wrapper<IMessageDispatcher> _dispatcher;
        Uuid _window_id = Uuid::NONE;
    };
}
