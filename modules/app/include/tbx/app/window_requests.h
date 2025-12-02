#pragma once
#include "tbx/messages/message.h"
#include "tbx/messages/window_description.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    class Window;

    // Request requesting a new platform window.
    struct TBX_API CreateWindowRequest : public Request<std::any>
    {
        CreateWindowRequest() = default;
        CreateWindowRequest(Window* window_ptr, WindowDescription desc);

        // Non-owning pointer to the window being created.
        Window* window = nullptr;
        WindowDescription description = {};
    };

    struct TBX_API QueryWindowDescriptionRequest : public Request<WindowDescription>
    {
        QueryWindowDescriptionRequest() = default;
        QueryWindowDescriptionRequest(Window* window_ptr);

        // Non-owning pointer to the window being queried.
        Window* window = nullptr;
    };

    struct TBX_API ApplyWindowDescriptionRequest : public Request<WindowDescription>
    {
        ApplyWindowDescriptionRequest() = default;
        ApplyWindowDescriptionRequest(Window* window_ptr, WindowDescription desc);

        // Non-owning pointer to the window to be updated.
        Window* window = nullptr;
        WindowDescription description = {};
    };

}
