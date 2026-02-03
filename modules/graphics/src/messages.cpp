#include "tbx/graphics/messages.h"

namespace tbx
{
    WindowMakeCurrentRequest::WindowMakeCurrentRequest(const Uuid& window_id)
        : window(window_id)
    {
        not_handled_behavior = MessageNotHandledBehavior::Assert;
    }

    WindowMakeCurrentRequest::~WindowMakeCurrentRequest() noexcept = default;

    WindowPresentRequest::WindowPresentRequest(const Uuid& window_id)
        : window(window_id)
    {
        not_handled_behavior = MessageNotHandledBehavior::Assert;
    }

    WindowPresentRequest::~WindowPresentRequest() noexcept = default;
}
