#include "tbx/app/window_requests.h"
#include <utility>

namespace tbx
{
    CreateWindowRequest::CreateWindowRequest(Window* window_ptr, WindowDescription desc)
        : window(window_ptr)
        , description(std::move(desc))
    {
    }

    QueryWindowDescriptionRequest::QueryWindowDescriptionRequest(Window* window_ptr)
        : window(window_ptr)
    {
    }

    ApplyWindowDescriptionRequest::ApplyWindowDescriptionRequest(
        Window* window_ptr,
        WindowDescription desc)
        : window(window_ptr)
        , description(std::move(desc))
    {
    }
}
