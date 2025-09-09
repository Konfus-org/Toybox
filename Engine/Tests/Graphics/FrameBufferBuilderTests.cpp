#include "PCH.h"
#include "Tbx/Graphics/Buffers.h"
#include "Tbx/Graphics/Camera.h"
#include "Tbx/Graphics/Material.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Graphics/FrameBufferBuilder.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Math/Transform.h"
#include "Tbx/TBS/Box.h"
#include <algorithm>

namespace Tbx::Tests::Graphics
{
    TEST(FrameBufferBuilderTests, BuildUploadBuffer_UploadsMaterialsAndMeshes)
    {
        // Arrange
        auto box = std::make_shared<Box>();
        auto toyA = box->EmplaceToy("ToyA");
        auto toyB = box->EmplaceToy("ToyB");
        auto toyC = box->EmplaceToy("ToyC");

        Material materialA;
        Texture textureA;
        MaterialInstance matInstanceA(materialA, textureA);

        Material materialB;
        Texture textureB;
        MaterialInstance matInstanceB(materialB, textureB);

        Mesh meshA;
        Mesh meshB;
        Mesh meshC;

        box->EmplaceBlockOn<MaterialInstance>(toyA, matInstanceA);
        box->EmplaceBlockOn<Mesh>(toyA, meshA);
        box->EmplaceBlockOn<MaterialInstance>(toyB, matInstanceA);
        box->EmplaceBlockOn<Mesh>(toyB, meshB);
        box->EmplaceBlockOn<MaterialInstance>(toyC, matInstanceB);
        box->EmplaceBlockOn<Mesh>(toyC, meshC);

        std::vector<std::shared_ptr<Box>> boxes{box};

        // Act
        FrameBufferBuilder builder;
        FrameBuffer buffer = builder.BuildUploadBuffer(boxes);

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

        EXPECT_EQ(uploadedMaterials.size(), 2);
        EXPECT_TRUE((uploadedMaterials[0] == matInstanceA.GetId() &&
                     uploadedMaterials[1] == matInstanceB.GetId()) ||
                    (uploadedMaterials[0] == matInstanceB.GetId() &&
                     uploadedMaterials[1] == matInstanceA.GetId()));

        EXPECT_EQ(uploadedMeshes.size(), 3);
        EXPECT_NE(std::find(uploadedMeshes.begin(), uploadedMeshes.end(), meshA.GetId()), uploadedMeshes.end());
        EXPECT_NE(std::find(uploadedMeshes.begin(), uploadedMeshes.end(), meshB.GetId()), uploadedMeshes.end());
        EXPECT_NE(std::find(uploadedMeshes.begin(), uploadedMeshes.end(), meshC.GetId()), uploadedMeshes.end());
    }

    TEST(FrameBufferBuilderTests, BuildRenderBuffer_NoCamera_ReturnsEmptyBuffer)
    {
        // Arrange
        auto box = std::make_shared<Box>();
        std::vector<std::shared_ptr<Box>> boxes{box};

        // Act
        FrameBufferBuilder builder;
        FrameBuffer buffer = builder.BuildRenderBuffer(boxes);

        // Assert
        EXPECT_TRUE(buffer.GetCommands().empty());
    }

    TEST(FrameBufferBuilderTests, BuildRenderBuffer_WithCamera_ProducesExpectedCommands)
    {
        // Arrange
        auto box = std::make_shared<Box>();

        // Camera setup
        auto camToy = box->EmplaceToy("CameraToy");
        Camera camera;
        Transform camTransform;
        box->EmplaceBlockOn<Camera>(camToy, camera);
        box->EmplaceBlockOn<Transform>(camToy, camTransform);

        // Visible toy setup
        auto toy = box->EmplaceToy("VisibleToy");
        Material material;
        Texture texture;
        MaterialInstance matInstance(material, texture);
        Mesh mesh;
        Transform transform;
        transform.Position = {0.0f, 0.0f, -5.0f};
        box->EmplaceBlockOn<MaterialInstance>(toy, matInstance);
        box->EmplaceBlockOn<Mesh>(toy, mesh);
        box->EmplaceBlockOn<Transform>(toy, transform);

        std::vector<std::shared_ptr<Box>> boxes{box};

        // Act
        FrameBufferBuilder builder;
        FrameBuffer buffer = builder.BuildRenderBuffer(boxes);

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

