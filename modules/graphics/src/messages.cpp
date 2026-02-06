#include "tbx/graphics/messages.h"

namespace tbx
{
    WindowMakeCurrentRequest::WindowMakeCurrentRequest(const Uuid& window_id)
        : window(window_id)
    {
        not_handled_behavior = MessageNotHandledBehavior::ASSERT;
    }

    WindowMakeCurrentRequest::~WindowMakeCurrentRequest() noexcept = default;

    WindowPresentRequest::WindowPresentRequest(const Uuid& window_id)
        : window(window_id)
    {
        not_handled_behavior = MessageNotHandledBehavior::ASSERT;
    }

    WindowPresentRequest::~WindowPresentRequest() noexcept = default;
}
