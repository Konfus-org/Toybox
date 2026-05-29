#include "tbx/systems/plugin_api/service_provider.h"

namespace tbx
{
    void ServiceProvider::deregister_service(std::type_index service_type)
    {
        _entries.erase(service_type);
    }

    void ServiceProvider::clear()
    {
        _entries.clear();
    }
}
