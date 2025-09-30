#include "PCH.h"
#include "Tbx/Stages/Toy.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx::Tests::Stages
{
    class CountingToy : public Toy
    {
    public:
        using Toy::Toy;

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
        auto parent = MakeRef<CountingToy>("Parent");
        auto child = MakeRef<CountingToy>("Child");
        parent->Children.push_back(child);

        // Act
        parent->Update();

        // Assert
        EXPECT_EQ(parent->UpdateCount, 1);
        EXPECT_EQ(child->UpdateCount, 1);
    }

    TEST(ToyTests, Update_SkipsWhenDisabled)
    {
        // Arrange
        auto parent = MakeRef<CountingToy>("Parent");
        auto child = MakeRef<CountingToy>("Child");
        child->Enabled = false;
        parent->Children.push_back(child);

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
        auto parent = MakeRef<CountingToy>("Parent");
        auto child = MakeRef<CountingToy>("Child");
        child->Enabled = false;
        parent->Children.push_back(child);

        // Act
        parent->Update();

        // Assert
        EXPECT_EQ(parent->UpdateCount, 1);
        EXPECT_EQ(child->UpdateCount, 0);
    }
}
