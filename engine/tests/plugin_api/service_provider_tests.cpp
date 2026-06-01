#include "tbx/systems/plugin_api/service_provider.h"
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace tbx::tests::plugin_api
{
    struct ServiceProviderTestService
    {
        virtual ~ServiceProviderTestService() = default;
    };

    struct ServiceProviderDependency final : ServiceProviderTestService
    {
        ServiceProviderDependency(std::vector<std::string>& destruction_order)
            : order(destruction_order)
        {
        }

        ~ServiceProviderDependency() noexcept override
        {
            order.get().push_back("dependency");
        }

        std::reference_wrapper<std::vector<std::string>> order;
    };

    struct ServiceProviderDependent final
    {
        ServiceProviderDependent(
            std::shared_ptr<ServiceProviderTestService> dependency_service,
            std::vector<std::string>& destruction_order)
            : dependency(std::move(dependency_service))
            , order(destruction_order)
        {
        }

        ~ServiceProviderDependent() noexcept
        {
            order.get().push_back("dependent");
        }

        std::shared_ptr<ServiceProviderTestService> dependency = {};
        std::reference_wrapper<std::vector<std::string>> order;
    };

    static_assert(!std::is_copy_constructible_v<ServiceProvider>);
    static_assert(!std::is_copy_assignable_v<ServiceProvider>);
    static_assert(!std::is_move_constructible_v<ServiceProvider>);
    static_assert(!std::is_move_assignable_v<ServiceProvider>);

    TEST(ServiceProviderTests, ClearExpiresWeakServiceReferences)
    {
        // Arrange
        auto service_provider = ServiceProvider {};
        service_provider.register_service<ServiceProviderTestService>(
            std::make_shared<ServiceProviderTestService>());
        std::weak_ptr<ServiceProviderTestService> service =
            service_provider.get_service<ServiceProviderTestService>();

        // Act
        service_provider.clear();

        // Assert
        EXPECT_TRUE(service.expired());
    }

    TEST(ServiceProviderTests, ClearDestroysServicesInReverseRegistrationOrder)
    {
        // Arrange
        auto service_provider = ServiceProvider {};
        auto destruction_order = std::vector<std::string>();
        service_provider.register_service<ServiceProviderTestService>(
            std::make_shared<ServiceProviderDependency>(destruction_order));
        service_provider.register_service<ServiceProviderDependent>(
            std::make_shared<ServiceProviderDependent>(
                service_provider.get_service<ServiceProviderTestService>().lock(),
                destruction_order));

        // Act
        service_provider.clear();

        // Assert
        ASSERT_EQ(destruction_order.size(), 2U);
        EXPECT_EQ(destruction_order[0], "dependent");
        EXPECT_EQ(destruction_order[1], "dependency");
    }

#if defined(TBX_ASSERTS_ENABLED)
    TEST(ServiceProviderTests, ClearAssertsWhenServiceHasExternalStrongReference)
    {
        // Arrange
        auto service_provider = ServiceProvider {};
        service_provider.register_service<ServiceProviderTestService>(
            std::make_shared<ServiceProviderTestService>());
        auto retained_service = service_provider.get_service<ServiceProviderTestService>().lock();
        ASSERT_NE(retained_service, nullptr);

        // Act / Assert
        EXPECT_DEATH_IF_SUPPORTED(service_provider.clear(), "");

        retained_service.reset();
        service_provider.clear();
    }
#endif
}
