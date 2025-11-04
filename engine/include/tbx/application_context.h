#pragma once
#include "tbx/app_description.h"
#include "tbx/service_locator.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    class Application;
    class IMessageDispatcher;

    struct TBX_API ApplicationContext
    {
        Application* instance = nullptr;
        AppDescription description = {};
        ServiceLocator* services = nullptr;
        IMessageDispatcher* dispatcher = nullptr;
    };

    namespace detail
    {
        inline const ApplicationContext*& current_application_context_slot()
        {
            static const ApplicationContext* global_context = nullptr;
            return global_context;
        }
    }

    inline const ApplicationContext* current_application_context()
    {
        return detail::current_application_context_slot();
    }

    inline const ApplicationContext* set_current_application_context(
        const ApplicationContext* context)
    {
        auto& slot = detail::current_application_context_slot();
        const ApplicationContext* previous = slot;
        slot = context;
        return previous;
    }
}
