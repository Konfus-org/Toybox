#include "tbx/systems/app/app_service_provider.h"

namespace tbx
{
    void AppServiceProvider::initialize()
    {
        if (!_provider)
            _provider = std::make_shared<ServiceProvider>();
    }

    bool AppServiceProvider::is_valid() const noexcept
    {
        return _provider != nullptr;
    }

    const std::shared_ptr<ServiceProvider>& AppServiceProvider::shared() const noexcept
    {
        return _provider;
    }

    ServiceProvider& AppServiceProvider::get() noexcept
    {
        return *_provider;
    }

    const ServiceProvider& AppServiceProvider::get() const noexcept
    {
        return *_provider;
    }

    void AppServiceProvider::reset() noexcept
    {
        _provider = {};
    }
}
