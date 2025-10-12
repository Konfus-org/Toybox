#include "PCH.h"
#include "Tbx/Stages/Toy.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx::Tests::Stages
{
    class CountingToy : public Toy
    {
    public:
        CountingToy() = default;
        explicit CountingToy(const std::string& name)
            : Toy(name)
        {
        }

        void OnUpdate() override
        {
            ++UpdateCount;
        }

        int UpdateCount = 0;
    };

    TEST(ToyTests, Constructor_AssignsNameToHandle)
    {
        // Act
        CountingToy toy("Player");

        // Assert
        EXPECT_EQ(toy.Handle.Name, "Player");
        EXPECT_TRUE(toy.Enabled);
    }

    TEST(ToyTests, Update_CallsOnUpdateWhenEnabled)
    {
        // Arrange
        auto parent = Toy::Make<CountingToy>("Parent");
        auto child = Toy::Make<CountingToy>("Child");
        parent->Children.Add(child);

        // Act
        parent->Update();

        // Assert
        EXPECT_EQ(parent->UpdateCount, 1);
        EXPECT_EQ(child->UpdateCount, 1);
    }

    TEST(ToyTests, Update_SkipsWhenDisabled)
    {
        // Arrange
        auto parent = Toy::Make<CountingToy>("Parent");
        auto child = Toy::Make<CountingToy>("Child");
        child->Enabled = false;
        parent->Children.Add(child);

        // Act
        parent->Enabled = false;
        parent->Update();

        // Assert
        EXPECT_EQ(parent->UpdateCount, 0);
        EXPECT_EQ(child->UpdateCount, 0);
    }

    TEST(ToyTests, Update_StillSkipsDisabledChildren)
    {
        // Arrange
        auto parent = Toy::Make<CountingToy>("Parent");
        auto child = Toy::Make<CountingToy>("Child");
        child->Enabled = false;
        parent->Children.Add(child);

        // Act
        parent->Update();

        // Assert
        EXPECT_EQ(parent->UpdateCount, 1);
        EXPECT_EQ(child->UpdateCount, 0);
    }

    TEST(ToyTests, Add_CreatesChildWhenToyTypeIsRequested)
    {
        auto parent = Toy::Make<CountingToy>("Parent");

        const auto child = parent->Add<CountingToy>("Child");

        ASSERT_NE(child, nullptr);
        EXPECT_TRUE(parent->Has<CountingToy>());
        EXPECT_EQ(parent->Children.Count(), 1ull);
        EXPECT_EQ(child->Handle.Name, "Child");
    }

    TEST(ToyTests, TryGet_ReturnsChildWhenPresent)
    {
        auto parent = Toy::Make<CountingToy>("Parent");
        const auto child = parent->Add<CountingToy>("Child");

        Ref<CountingToy> retrieved;
        EXPECT_TRUE(parent->TryGet(retrieved));
        EXPECT_EQ(retrieved, child);
    }
}
