#include "PCH.h"
#include "Tbx/Stages/Stage.h"
#include "Tbx/Stages/Toy.h"
#include "Tbx/Memory/Refs.h"
#include <string>

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

    TEST(StageTests, Constructor_CreatesRootToy)
    {
        // Act
        auto stage = Stage::Make();

        // Assert
        auto root = stage->Root;
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Handle.Name, "Root");
    }

    TEST(StageTests, Update_PropagatesToChildren)
    {
        // Arrange
        auto stage = Stage::Make();
        auto child = stage->Add<CountingToy>("Child");

        // Act
        stage->Update();

        // Assert
        EXPECT_EQ(child->UpdateCount, 1);
    }

    TEST(StageTests, Update_SkipsDisabledChildren)
    {
        // Arrange
        auto stage = Stage::Make();
        auto child = stage->Add<CountingToy>("Child");
        child->Enabled = false;
        stage->Root->Children.Add(child);

        // Act
        stage->Update();

        // Assert
        EXPECT_EQ(child->UpdateCount, 0);
    }
}
