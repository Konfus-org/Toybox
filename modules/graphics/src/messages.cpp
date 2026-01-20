#include "tbx/graphics/messages.h"

namespace tbx
{
    WindowMakeCurrentRequest::WindowMakeCurrentRequest(const Uuid& window_id)
        : window(window_id)
    {
        require_handling = true;
    }

    WindowMakeCurrentRequest::~WindowMakeCurrentRequest() noexcept = default;

    WindowPresentRequest::WindowPresentRequest(const Uuid& window_id)
        : window(window_id)
    {
        require_handling = true;
    }

    WindowPresentRequest::~WindowPresentRequest() noexcept = default;
}
