#include "PCH.h"
#include "Tbx/Stages/Stage.h"
#include "Tbx/Stages/Views.h"
#include "Tbx/Stages/Blocks.h"
#include "Tbx/Math/Transform.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Memory/Refs.h"
#include <gmock/gmock.h>
#include <string>
#include <vector>

namespace Tbx::Tests::Stages
{
    TEST(StageViewTests, IteratesAllToysWhenNoFilter)
    {
        // Arrange
        Stage stage;
        auto root = stage.GetRoot();
        auto childA = MakeRef<Toy>("ChildA");
        auto childB = MakeRef<Toy>("ChildB");
        auto grandChild = MakeRef<Toy>("GrandChild");

        childA->Children.push_back(grandChild);
        root->Children.push_back(childA);
        root->Children.push_back(childB);

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
        Stage stage;
        auto root = stage.GetRoot();
        auto childA = MakeRef<Toy>("ChildA");
        auto childB = MakeRef<Toy>("ChildB");
        auto grandChild = MakeRef<Toy>("GrandChild");

        childA->Blocks.Add<Transform>();
        childB->Blocks.Add<Mesh>();
        grandChild->Blocks.Add<Mesh>();

        childA->Children.push_back(grandChild);
        root->Children.push_back(childA);
        root->Children.push_back(childB);

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
        Stage stage;
        auto root = stage.GetRoot();
        auto childA = MakeRef<Toy>("ChildA");
        auto childB = MakeRef<Toy>("ChildB");

        childA->Blocks.Add<Transform>();
        childB->Blocks.Add<Mesh>();

        root->Children.push_back(childA);
        root->Children.push_back(childB);

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
