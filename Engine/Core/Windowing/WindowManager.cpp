#include "TbxPCH.h"
#include "WindowManager.h"
#include "Events/Events.h"
#include "Debug/DebugAPI.h"

namespace Tbx
{
    UID WindowManager::OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size)
    {
        auto event = OpenNewWindowEvent(name, mode, size);
        Events::Send<OpenNewWindowEvent>(event);

        TBX_ASSERT(event.Handled, "Failed to open new window with the name {}!", name);
        TBX_ASSERT(event.GetResult() != -1, "No id returned when opening new window with the name {}!", name);

        return event.GetResult();
    }

    std::weak_ptr<IWindow> WindowManager::GetWindow(const uint64& id)
    {
        auto event = GetWindowEvent(id);
        Events::Send<GetWindowEvent>(event);

        TBX_ASSERT(event.Handled, "Failed to get window with the id {}!", std::to_string(id));
        TBX_VALIDATE_PTR(event.GetResult().lock(), "No result returned when querying for window with the id {}!", std::to_string(id));

        return event.GetResult();
    }
}
