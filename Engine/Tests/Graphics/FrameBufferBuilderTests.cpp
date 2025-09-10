#include "PCH.h"
#include "Tbx/Graphics/Buffers.h"
#include "Tbx/Graphics/Camera.h"
#include "Tbx/Graphics/Material.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Graphics/FrameBufferBuilder.h"
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
        auto toyA = root->EmplaceChild("ToyA");
        auto toyB = root->EmplaceChild("ToyB");
        auto toyC = root->EmplaceChild("ToyC");

        auto& meshA = toyA->EmplaceBlock<Mesh>();
        auto& meshB = toyB->EmplaceBlock<Mesh>();
        auto& meshC = toyC->EmplaceBlock<Mesh>();
        auto& matInstanceA = toyA->EmplaceBlock<MaterialInstance>(materialA, textureA);
        auto& matInstanceB = toyB->EmplaceBlock<MaterialInstance>(materialA, textureB);
        auto& matInstanceC = toyC->EmplaceBlock<MaterialInstance>(materialB, textureB);

        // Act
        FrameBufferBuilder builder;
        FrameBuffer buffer = builder.BuildUploadBuffer(root);

        // Assert
        std::vector<Uid> uploadedMaterials;
        std::vector<Uid> uploadedMeshes;
        for (const auto& cmd : buffer.GetCommands())
        {
            if (cmd.GetType() == DrawCommandType::UploadMaterial)
            {
                const auto& mat = std::any_cast<const MaterialInstance&>(cmd.GetPayload());
                uploadedMaterials.push_back(mat.GetId());
            }
            if (cmd.GetType() == DrawCommandType::UploadMesh)
            {
                const auto& mesh = std::any_cast<const Mesh&>(cmd.GetPayload());
                uploadedMeshes.push_back(mesh.GetId());
            }
        }

        EXPECT_EQ(uploadedMaterials.size(), 3);
        EXPECT_NE(std::find(uploadedMaterials.begin(), uploadedMaterials.end(), matInstanceA.GetId()), uploadedMaterials.end());
        EXPECT_NE(std::find(uploadedMaterials.begin(), uploadedMaterials.end(), matInstanceB.GetId()), uploadedMaterials.end());
        EXPECT_NE(std::find(uploadedMaterials.begin(), uploadedMaterials.end(), matInstanceC.GetId()), uploadedMaterials.end());

        EXPECT_EQ(uploadedMeshes.size(), 3);
        EXPECT_NE(std::find(uploadedMeshes.begin(), uploadedMeshes.end(), meshA.GetId()), uploadedMeshes.end());
        EXPECT_NE(std::find(uploadedMeshes.begin(), uploadedMeshes.end(), meshB.GetId()), uploadedMeshes.end());
        EXPECT_NE(std::find(uploadedMeshes.begin(), uploadedMeshes.end(), meshC.GetId()), uploadedMeshes.end());
    }

    TEST(FrameBufferBuilderTests, BuildRenderBuffer_NoCamera_ReturnsEmptyBuffer)
    {
        // Arrange
        auto toy = std::make_shared<Toy>();

        // Act
        FrameBufferBuilder builder;
        FrameBuffer buffer = builder.BuildRenderBuffer(toy);

        // Assert
        EXPECT_TRUE(buffer.GetCommands().empty());
    }

    TEST(FrameBufferBuilderTests, BuildRenderBuffer_WithCamera_ProducesExpectedCommandsInCorrectOrder)
    {
        // Arrange
        Material material = {};
        Texture texture = {};
        auto root = std::make_shared<Toy>();

        // Camera setup
        auto camToy = root->EmplaceChild("CameraToy");
        camToy->EmplaceBlock<Camera>();
        camToy->EmplaceBlock<Transform>();

        // Visible toy setup
        auto visibleToy = root->EmplaceChild("VisibleToy");
        auto& matInstance = visibleToy->EmplaceBlock<MaterialInstance>(material, texture);
        auto& mesh = visibleToy->EmplaceBlock<Mesh>();
        visibleToy->EmplaceBlock<Transform>()
            .SetPosition({ 0.0f, 0.0f, 5.0f });

        // Act
        FrameBufferBuilder builder;
        FrameBuffer buffer = builder.BuildRenderBuffer(root);

        // Assert
        const auto& cmds = buffer.GetCommands();
        ASSERT_EQ(cmds.size(), 5);

        const auto& cmd0Uniform = std::any_cast<const ShaderUniform&>(cmds[0].GetPayload());
        EXPECT_EQ(cmds[0].GetType(), DrawCommandType::SetUniform);
        EXPECT_STREQ(cmd0Uniform.Name.c_str(), "TransformUniform");

        const auto& cmd1Uniform = std::any_cast<const ShaderUniform&>(cmds[1].GetPayload());
        EXPECT_EQ(cmds[1].GetType(), DrawCommandType::SetUniform);
        EXPECT_STREQ(cmd1Uniform.Name.c_str(), "ViewProjectionUniform");

        EXPECT_EQ(cmds[2].GetType(), DrawCommandType::SetMaterial);
        const auto& cmd2Material = std::any_cast<const MaterialInstance&>(cmds[2].GetPayload());
        EXPECT_EQ(cmd2Material.GetId(), matInstance.GetId());

        const auto& cmd3Uniform = std::any_cast<const ShaderUniform&>(cmds[3].GetPayload());
        EXPECT_EQ(cmds[3].GetType(), DrawCommandType::SetUniform);
        EXPECT_STREQ(cmd3Uniform.Name.c_str(), "TransformUniform");

        EXPECT_EQ(cmds[4].GetType(), DrawCommandType::DrawMesh);
        const auto& cmd4Mesh = std::any_cast<const Mesh&>(cmds[4].GetPayload());
        EXPECT_EQ(cmd4Mesh.GetId(), mesh.GetId());

    }
}

