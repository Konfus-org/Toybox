#include "tbx/service_locator.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

namespace tbx::tests
{
    struct DummyService
    {
        explicit DummyService(int v) : value(v) {}
        int value = 0;
    };

    TEST(ServiceLocatorTests, EmplaceAndGet)
    {
        ServiceLocator locator;
        auto& service = locator.emplace<DummyService>(123);

        EXPECT_TRUE(locator.contains<DummyService>());
        EXPECT_EQ(123, service.value);
        EXPECT_EQ(&service, locator.try_get<DummyService>());
        EXPECT_EQ(123, locator.get<DummyService>().value);
    }

    TEST(ServiceLocatorTests, AddSharedService)
    {
        ServiceLocator locator;
        auto service = std::make_shared<DummyService>(77);
        locator.add(service);

        EXPECT_TRUE(locator.contains<DummyService>());
        EXPECT_EQ(77, locator.get<DummyService>().value);
    }

    TEST(ServiceLocatorTests, ClearRemovesServices)
    {
        ServiceLocator locator;
        locator.emplace<DummyService>(55);
        locator.clear();

        EXPECT_FALSE(locator.contains<DummyService>());
        EXPECT_EQ(nullptr, locator.try_get<DummyService>());
    }

    TEST(ServiceLocatorTests, GetThrowsWhenMissing)
    {
        ServiceLocator locator;
        EXPECT_THROW(static_cast<void>(locator.get<DummyService>()), std::runtime_error);
    }

    TEST(ServiceLocatorTests, AddRejectsNullSharedPtr)
    {
        ServiceLocator locator;
        std::shared_ptr<DummyService> null_service;
        EXPECT_THROW(locator.add(null_service), std::invalid_argument);
    }
}
