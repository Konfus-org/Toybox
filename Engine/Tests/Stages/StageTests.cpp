#include "PCH.h"
#include "Tbx/Stages/Stage.h"
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

    TEST(StageTests, Constructor_CreatesRootToy)
    {
        // Act
        Stage stage;

        // Assert
        auto root = stage.GetRoot();
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Handle.Name, "Root");
    }

    TEST(StageTests, Update_PropagatesToChildren)
    {
        // Arrange
        Stage stage;
        auto child = MakeRef<CountingToy>("Child");
        stage.GetRoot()->Children.push_back(child);

        // Act
        stage.Update();

        // Assert
        EXPECT_EQ(child->UpdateCount, 1);
    }

    TEST(StageTests, Update_SkipsDisabledChildren)
    {
        // Arrange
        Stage stage;
        auto child = MakeRef<CountingToy>("Child");
        child->Enabled = false;
        stage.GetRoot()->Children.push_back(child);

        // Act
        stage.Update();

        // Assert
        EXPECT_EQ(child->UpdateCount, 0);
    }
}
