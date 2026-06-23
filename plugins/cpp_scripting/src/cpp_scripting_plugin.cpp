#include "cpp_scripting_plugin.h"
#include "tbx/cpp_scripting/cpp_scripting_backend.h"
#include "tbx/systems/debugging/macros.h"
#include <memory>

namespace cpp_scripting
{
    void CppScripting::on_attach()
    {
        const auto registry = scripting_registry.lock();
        if (!registry)
        {
            TBX_TRACE_ERROR("CppScripting plugin attached without a ScriptingRegistry service.");
            return;
        }

        const auto result = registry->register_backend(
            std::make_shared<CppScriptingBackend>(asset_manager),
            get_id());
        if (!result.succeeded())
            TBX_TRACE_ERROR("Failed to register the C++ scripting backend: {}", result.get_report());
    }

    void CppScripting::on_detach()
    {
        // Drop the backend before this module unloads. ScriptSystem destroys its script instances
        // earlier in shutdown, so nothing still references the backend by the time we deregister.
        if (const auto registry = scripting_registry.lock())
            registry->unregister_all(get_id());
    }
}
