#include "PCH.h"
#include "Tbx/Stages/Stage.h"
#include "Tbx/Stages/Views.h"
#include "Tbx/Math/Transform.h"
#include "Tbx/Graphics/Mesh.h"
#include <gmock/gmock-matchers.h>
#include <string>
#include <vector>

namespace Tbx::Tests::Stages
{
    TEST(StageViewTests, IteratesAllToysWhenNoFilter)
    {
        // Arrange
        auto stage = Stage::Make();
        auto root = stage->Root;
        auto childA = stage->Add("ChildA");
        auto childB = stage->Add("ChildB");
        auto grandChild = childA->Add("GrandChild");

        // Act
        StageView<> view(root);
        std::vector<std::string> names;
        for (const auto& toy : view)
        {
            names.push_back(toy->Handle.Name);
        }

        // Assert
        ASSERT_EQ(names.size(), 4u);
        EXPECT_EQ(names[0], "Root");
        EXPECT_EQ(names[1], "ChildA");
        EXPECT_EQ(names[2], "GrandChild");
        EXPECT_EQ(names[3], "ChildB");
    }

    TEST(StageViewTests, FiltersToysByBlockType)
    {
        // Arrange
        auto stage = Stage::Make();
        auto root = stage->Root;
        auto childA = stage->Add("ChildA");
        auto childB = stage->Add("ChildB");
        auto grandChild = childA->Add("GrandChild");

        childA->Add<Transform>();
        childB->Add<Mesh>();
        grandChild->Add<Mesh>();

        // Act
        StageView<Mesh> view(root);
        std::vector<std::string> names;
        for (const auto& toy : view)
        {
            names.push_back(toy->Handle.Name);
        }

        // Assert
        ASSERT_EQ(names.size(), 2u);
        EXPECT_EQ(names[0], "GrandChild");
        EXPECT_EQ(names[1], "ChildB");
    }

    TEST(StageViewTests, IncludesToysMatchingAnyRequestedBlock)
    {
        // Arrange
        auto stage = Stage::Make();
        auto root = stage->Root;
        auto childA = stage->Add("ChildA");
        auto childB = stage->Add("ChildB");

        childA->Add<Transform>();
        childB->Add<Mesh>();

        // Act
        StageView<Transform, Mesh> view(root);
        std::vector<std::string> names;
        for (const auto& toy : view)
        {
            names.push_back(toy->Handle.Name);
        }

        // Assert
        ASSERT_EQ(names.size(), 2u);
        EXPECT_THAT(names, ::testing::ElementsAre("ChildA", "ChildB"));
    }
}
