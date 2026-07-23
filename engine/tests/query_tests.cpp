#include "reflection/reflection_internal.h"
#include "serialization/serializers_internal.h"
#include "tbx/app.h"
#include "tbx/ecs/query.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/gfx/camera.h"
#include "tbx/platform/window.h"
#include "tbx/reflection/reflection.h"
#include "tbx/serialization/serializers.h"
#include <functional>
#include <gtest/gtest.h>
#include <string>
#include <typeinfo>
#include <vector>

// Query surface tests. Each test drives exactly ONE endpoint and assumes the rest is fine. All
// headless: the world-backed sources (Toy/Window) are exercised by constructing Query<T> from a
// snapshot (never through run(), so NO OS windows open); the registry-backed sources
// (TypeInfo/SerializerInfo) read the process-global tables directly.
namespace tbx
{
    // A type the test never registers — its descriptor lookups must miss.
    struct NeverRegistered
    {
    };

    // Guarantees a clean, fully-registered reflection + serializer state regardless of what earlier
    // tests did to the process-global registries (a register_type before initialize_reflection would
    // otherwise leave the builtins unregistered — a known init-latch quirk).
    static void reset_type_system()
    {
        internal::purge_reflection_registry();
        internal::initialize_reflection();
        internal::purge_serialization_registry();
        internal::register_builtin_serializers();
    }

    //// Query<T> machinery — world-backed element types (no runtime, no windows) ////

    TEST(Query, WhereKeepsOnlyMatchingItems)
    {
        // Arrange
        auto sandbox = Sandbox();
        sandbox.spawn("Keep");
        sandbox.spawn("Drop");

        // Act
        auto kept =
            Query<Toy>(sandbox.get_toys()).where([](const Toy& toy) { return toy.get_name() == "Keep"; });

        // Assert
        ASSERT_EQ(kept.count(), size(1));
        EXPECT_EQ(kept.first()->get_name(), "Keep");
    }

    TEST(Query, WithKeepsOnlyToysCarryingEveryBlock)
    {
        // Arrange
        auto sandbox = Sandbox();
        sandbox.spawn("Eye").with(Camera());
        sandbox.spawn("Plain");

        // Act
        auto cameras = Query<Toy>(sandbox.get_toys()).with<Camera>();

        // Assert
        ASSERT_EQ(cameras.count(), size(1));
        EXPECT_EQ(cameras.first()->get_name(), "Eye");
    }

    TEST(Query, CountReportsHowManyRemain)
    {
        // Arrange
        auto sandbox = Sandbox();
        sandbox.spawn("A");
        sandbox.spawn("B");
        sandbox.spawn("C");

        // Act / Assert
        EXPECT_EQ(Query<Toy>(sandbox.get_toys()).count(), size(3));
    }

    TEST(Query, ToVectorMaterializesTheRemainingItems)
    {
        // Arrange
        auto sandbox = Sandbox();
        sandbox.spawn("Only");

        // Act
        const std::vector<Toy> toys = Query<Toy>(sandbox.get_toys()).to_vector();

        // Assert
        ASSERT_EQ(toys.size(), size(1));
        EXPECT_EQ(toys.front().get_name(), "Only");
    }

    TEST(Query, FirstOfAnEmptyQueryIsAbsent)
    {
        // Act / Assert: the negative case — nothing to return.
        EXPECT_FALSE(Query<Toy>(std::vector<Toy>()).first().has_value());
    }

    TEST(Query, WindowFirstReturnsAPointerForTheNonCopyableElement)
    {
        // Arrange: Windows are move-only plain data (constructing them opens NO OS window).
        auto storage = std::vector<Window>();
        storage.emplace_back().title = "Main";
        auto refs = std::vector<std::reference_wrapper<const Window>>();
        for (const Window& window : storage)
            refs.emplace_back(window);

        // Act
        const Window* first = Query<Window>(std::move(refs)).first();

        // Assert
        ASSERT_NE(first, nullptr);
        EXPECT_EQ(first->title, "Main");
    }

    TEST(Query, IterationYieldsUnwrappedElementReferences)
    {
        // Arrange
        auto storage = std::vector<Window>();
        storage.emplace_back().title = "Alpha";
        storage.emplace_back().title = "Beta";
        auto refs = std::vector<std::reference_wrapper<const Window>>();
        for (const Window& window : storage)
            refs.emplace_back(window);

        // Act: range-for must yield `const Window&`, not the stored reference_wrapper.
        auto seen = std::vector<std::string>();
        for (const Window& window : Query<Window>(std::move(refs)))
            seen.push_back(window.title);

        // Assert
        ASSERT_EQ(seen.size(), size(2));
        EXPECT_EQ(seen.front(), "Alpha");
    }

    //// Registry-backed sources + root-level helpers (process-global, headless) ////

    TEST(Query, GetAllTypeInfoEnumeratesTheReflectionRegistry)
    {
        // Arrange
        reset_type_system();

        // Act: a known builtin must appear among all registered types.
        bool found_app = false;
        for (const TypeInfo& type : get_all<TypeInfo>())
            if (type.name == "App")
                found_app = true;

        // Assert
        EXPECT_TRUE(found_app);
    }

    TEST(Query, GetAllSerializerInfoEnumeratesTheSerializerRegistry)
    {
        // Arrange
        reset_type_system();

        // Act: the registry has serializers, and App's type_hash is among them.
        bool found_app = false;
        for (const SerializerInfo& serializer : get_all<SerializerInfo>())
            if (serializer.type_hash == typeid(App).hash_code())
                found_app = true;

        // Assert
        EXPECT_TRUE(found_app);
    }

    TEST(Query, GetWhereFiltersTheCollection)
    {
        // Arrange
        reset_type_system();

        // Act
        auto app_only =
            get_where<TypeInfo>([](const TypeInfo& type) { return type.name == "App"; });

        // Assert
        ASSERT_EQ(app_only.count(), size(1));
        EXPECT_EQ(app_only.first()->name, "App");
    }

    TEST(Query, GetTypeFindsARegisteredType)
    {
        // Arrange
        reset_type_system();

        // Act
        const auto type = get_type<App>();

        // Assert
        ASSERT_TRUE(type.has_value());
        EXPECT_EQ(type->get().name, "App");
    }

    TEST(Query, GetTypeMissesAnUnregisteredType)
    {
        // Arrange
        reset_type_system();

        // Act / Assert: the negative case.
        EXPECT_FALSE(get_type<NeverRegistered>().has_value());
    }

    TEST(Query, GetSerializerFindsARegisteredType)
    {
        // Arrange
        reset_type_system();

        // Act
        const auto serializer = get_serializer<App>();

        // Assert
        ASSERT_TRUE(serializer.has_value());
        EXPECT_EQ(serializer->get().type_hash, typeid(App).hash_code());
    }

    TEST(Query, GetSerializerMissesAnUnregisteredType)
    {
        // Arrange
        reset_type_system();

        // Act / Assert: the negative case.
        EXPECT_FALSE(get_serializer<NeverRegistered>().has_value());
    }
}
