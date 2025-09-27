#include "PCH.h"
#include "Tbx/Graphics/Camera.h"
#include "Tbx/Graphics/Material.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Graphics/RenderCommands.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Math/Transform.h"
#include <algorithm>

namespace Tbx::Tests::Graphics
{
    TEST(FrameBufferBuilderTests, BuildUploadBuffer_UploadsMaterialsAndMeshes)
    {
        // Arrange
        Material materialA = {};
        Texture textureA = {};
        Material materialB = {};
        Texture textureB = {};

        auto root = std::make_shared<Toy>();
        auto toyA = root->Children.emplace_back("ToyA");
        auto toyB = root->Children.emplace_back("ToyB");
        auto toyC = root->Children.emplace_back("ToyC");

        auto& meshA = toyA->Blocks.Add<Mesh>();
        auto& meshB = toyB->Blocks.Add<Mesh>();
        auto& meshC = toyC->Blocks.Add<Mesh>();
        auto& matInstanceA = toyA->Blocks.Add<MaterialInstance>(materialA, std::vector<Tbx::Texture>({ textureA }));
        auto& matInstanceB = toyB->Blocks.Add<MaterialInstance>(materialA, std::vector<Tbx::Texture>({ textureB }));
        auto& matInstanceC = toyC->Blocks.Add<MaterialInstance>(materialB, std::vector<Tbx::Texture>({ textureB }));

        // Act
        RenderCommandBufferBuilder builder;
        RenderCommandBuffer buffer = builder.BuildUploadBuffer(root);

        // Assert
        std::vector<Uid> uploadedMaterials;
        std::vector<Uid> uploadedMeshes;
        for (const auto& cmd : buffer.Commands)
        {
            if (cmd.Type == RenderCommandType::UploadMaterial)
            {
                const auto& mat = std::any_cast<const MaterialInstance&>(cmd.Payload);
                uploadedMaterials.push_back(mat.Id);
            }
            if (cmd.Type == RenderCommandType::UploadMesh)
            {
                const auto& mesh = std::any_cast<const Mesh&>(cmd.Payload);
                uploadedMeshes.push_back(mesh.Id);
            }
        }

        EXPECT_EQ(uploadedMaterials.size(), 3);
        EXPECT_NE(std::find(uploadedMaterials.begin(), uploadedMaterials.end(), matInstanceA.Id), uploadedMaterials.end());
        EXPECT_NE(std::find(uploadedMaterials.begin(), uploadedMaterials.end(), matInstanceB.Id), uploadedMaterials.end());
        EXPECT_NE(std::find(uploadedMaterials.begin(), uploadedMaterials.end(), matInstanceC.Id), uploadedMaterials.end());

        EXPECT_EQ(uploadedMeshes.size(), 3);
        EXPECT_NE(std::find(uploadedMeshes.begin(), uploadedMeshes.end(), meshA.Id), uploadedMeshes.end());
        EXPECT_NE(std::find(uploadedMeshes.begin(), uploadedMeshes.end(), meshB.Id), uploadedMeshes.end());
        EXPECT_NE(std::find(uploadedMeshes.begin(), uploadedMeshes.end(), meshC.Id), uploadedMeshes.end());
    }

    TEST(FrameBufferBuilderTests, BuildRenderBuffer_NoCamera_ReturnsEmptyBuffer)
    {
        // Arrange
        auto toy = std::make_shared<Toy>();

        // Act
        RenderCommandBufferBuilder builder;
        RenderCommandBuffer buffer = builder.BuildRenderBuffer(toy);

        // Assert
        EXPECT_TRUE(buffer.Commands.empty());
    }

    TEST(FrameBufferBuilderTests, BuildRenderBuffer_WithCamera_ProducesExpectedCommandsInCorrectOrder)
    {
        // Arrange
        Material material = {};
        Texture texture = {};
        auto root = std::make_shared<Toy>();

        // Camera setup
        auto camToy = root->Children.emplace_back("CameraToy");
        camToy->Blocks.Add<Camera>();
        camToy->Blocks.Add<Transform>();

        // Visible toy setup
        auto visibleToy = root->Children.emplace_back("VisibleToy");
        auto& matInstance = visibleToy->Blocks.Add<MaterialInstance>(material, texture);
        auto& mesh = visibleToy->Blocks.Add<Mesh>();
        visibleToy->Blocks.Add<Transform>()
            .SetPosition({ 0.0f, 0.0f, 5.0f });

        // Act
        RenderCommandBufferBuilder builder;
        RenderCommandBuffer buffer = builder.BuildRenderBuffer(root);

        // Assert
        const auto& cmds = buffer.Commands;
        ASSERT_EQ(cmds.size(), 5);

        const auto& cmd0Uniform = std::any_cast<const ShaderUniform&>(cmds[0].Payload);
        EXPECT_EQ(cmds[0].Type, RenderCommandType::SetUniform);
        EXPECT_STREQ(cmd0Uniform.Name.c_str(), "TransformUniform");

        const auto& cmd1Uniform = std::any_cast<const ShaderUniform&>(cmds[1].Payload);
        EXPECT_EQ(cmds[1].Type, RenderCommandType::SetUniform);
        EXPECT_STREQ(cmd1Uniform.Name.c_str(), "ViewProjectionUniform");

        EXPECT_EQ(cmds[2].Type, RenderCommandType::SetMaterial);
        const auto& cmd2Material = std::any_cast<const MaterialInstance&>(cmds[2].Payload);
        EXPECT_EQ(cmd2Material.Id, matInstance.Id);

        const auto& cmd3Uniform = std::any_cast<const ShaderUniform&>(cmds[3].Payload);
        EXPECT_EQ(cmds[3].Type, RenderCommandType::SetUniform);
        EXPECT_STREQ(cmd3Uniform.Name.c_str(), "TransformUniform");

        EXPECT_EQ(cmds[4].Type, RenderCommandType::DrawMesh);
        const auto& cmd4Mesh = std::any_cast<const Mesh&>(cmds[4].Payload);
        EXPECT_EQ(cmd4Mesh.Id, mesh.Id);
    }
}

