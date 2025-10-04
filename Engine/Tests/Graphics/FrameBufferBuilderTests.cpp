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
        Ref<Material> materialA = MakeRef<Material>();
        Ref<Texture> textureA = MakeRef<Texture>();
        Ref<Material> materialB = MakeRef<Material>();
        Ref<Texture> textureB = MakeRef<Texture>();

        auto root = MakeRef<Toy>();
        auto toyA = MakeRef<Toy>("ToyA");
        auto toyB = MakeRef<Toy>("ToyB");
        auto toyC = MakeRef<Toy>("ToyC");
        root->Children.push_back(toyA);
        root->Children.push_back(toyB);
        root->Children.push_back(toyC);

        auto& meshA = toyA->Blocks.Add<Mesh>();
        auto& meshB = toyB->Blocks.Add<Mesh>();
        auto& meshC = toyC->Blocks.Add<Mesh>();
        auto& matInstanceA = toyA->Blocks.Add<MaterialInstance>(materialA, textureA);
        auto& matInstanceB = toyB->Blocks.Add<MaterialInstance>(materialA, textureB);
        auto& matInstanceC = toyC->Blocks.Add<MaterialInstance>(materialB, textureB);

        // Act
        DrawCommandBufferBuilder builder;
        DrawCommandBuffer buffer = builder.BuildUploadBuffer(root);

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
        auto toy = MakeRef<Toy>();

        // Act
        DrawCommandBufferBuilder builder;
        DrawCommandBuffer buffer = builder.Build(toy, 1.0f);

        // Assert
        EXPECT_TRUE(buffer.Commands.empty());
    }

    TEST(FrameBufferBuilderTests, BuildRenderBuffer_WithCamera_ProducesExpectedCommandsInCorrectOrder)
    {
        // Arrange
        Ref<Material> material = MakeRef<Material>();
        Ref<Texture> texture = MakeRef<Texture>();

        auto root = MakeRef<Toy>();
        auto camToy = MakeRef<Toy>("CameraToy");
        camToy->Blocks.Add<Camera>();
        camToy->Blocks.Add<Transform>();

        auto visibleToy = MakeRef<Toy>("VisibleToy");
        const auto& matInstance = visibleToy->Blocks.Add<MaterialInstance>(material, texture);
        const auto& mesh = visibleToy->Blocks.Add<Mesh>();
        visibleToy->Blocks.Add<Transform>().SetPosition({ 0.0f, 0.0f, 5.0f });

        root->Children.push_back(visibleToy);
        root->Children.push_back(camToy);

        // Act
        DrawCommandBufferBuilder builder;
        DrawCommandBuffer buffer = builder.Build(root, 1.0f);

        // Assert
        const auto& cmds = buffer.Commands;
        ASSERT_EQ(cmds.size(), 4);

        EXPECT_EQ(cmds[0].Type, RenderCommandType::SetMaterial);
        const auto& cmd0Material = std::any_cast<const MaterialInstance&>(cmds[0].Payload);
        EXPECT_EQ(cmd0Material.Id, matInstance.Id);

        EXPECT_EQ(cmds[1].Type, RenderCommandType::SetUniform);
        const auto& cmd1Uniform = std::any_cast<const ShaderUniform&>(cmds[1].Payload);
        EXPECT_STREQ(cmd1Uniform.Name.c_str(), "TransformUniform");

        EXPECT_EQ(cmds[2].Type, RenderCommandType::DrawMesh);
        const auto& cmd2Mesh = std::any_cast<const Mesh&>(cmds[2].Payload);
        EXPECT_EQ(cmd2Mesh.Id, mesh.Id);

        EXPECT_EQ(cmds[3].Type, RenderCommandType::SetUniform);
        const auto& cmd3Uniform = std::any_cast<const ShaderUniform&>(cmds[3].Payload);
        EXPECT_STREQ(cmd3Uniform.Name.c_str(), "ViewProjectionUniform");
    }
}

