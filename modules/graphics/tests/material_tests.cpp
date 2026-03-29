#include "PCH.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/common/handle.h"
#include "tbx/graphics/material.h"

namespace tbx::tests::graphics
{
    // Validates that a PBR material instance stores overrides using generated key constants.
    TEST(MaterialTests, PbrMaterialInstance_Overrides_AreStoredByGeneratedKeys)
    {
        // Arrange
        auto material = MaterialInstance(PbrMaterial::HANDLE);
        material.set_parameter(PbrMaterial::COLOR, Color(0.25f, 0.5f, 0.75f, 1.0f));
        material.set_parameter(PbrMaterial::DIFFUSE_STRENGTH, 0.9f);
        material.set_parameter(PbrMaterial::EMISSIVE_STRENGTH, 0.5f);
        material.set_texture(PbrMaterial::DIFFUSE_MAP, CheckerboardTexture::HANDLE);
        material.set_texture(PbrMaterial::NORMAL_MAP, Handle("Textures/NeutralNormal.png"));

        // Act
        const auto color = material.get_parameter_or(PbrMaterial::COLOR, Color::BLACK);
        const auto diffuse_strength = material.get_float_parameter_or(PbrMaterial::DIFFUSE_STRENGTH, 0.0f);
        const auto emissive_strength =
            material.get_float_parameter_or(PbrMaterial::EMISSIVE_STRENGTH, 0.0f);
        const auto diffuse_map = material.get_texture_handle_or(PbrMaterial::DIFFUSE_MAP);
        const auto normal_map = material.get_texture_handle_or(PbrMaterial::NORMAL_MAP);

        // Assert
        EXPECT_EQ(material.get_handle().id, PbrMaterial::HANDLE.id);
        EXPECT_FLOAT_EQ(color.r, 0.25f);
        EXPECT_FLOAT_EQ(color.g, 0.5f);
        EXPECT_FLOAT_EQ(color.b, 0.75f);
        EXPECT_FLOAT_EQ(diffuse_strength, 0.9f);
        EXPECT_FLOAT_EQ(emissive_strength, 0.5f);
        EXPECT_EQ(diffuse_map.id, CheckerboardTexture::HANDLE.id);
        EXPECT_EQ(normal_map.name, "Textures/NeutralNormal.png");
    }

    // Validates that depth overrides are tracked separately from material asset config.
    TEST(MaterialTests, MaterialInstance_SetDepth_EnablesDepthOverride)
    {
        // Arrange
        auto material = MaterialInstance(PbrMaterial::HANDLE);
        material.clear_dirty();

        // Act
        material.set_depth(Depth {
            .is_test_enabled = false,
            .is_write_enabled = false,
            .is_prepass_enabled = true,
            .function = MaterialDepthFunction::Always,
        });

        // Assert
        EXPECT_TRUE(material.has_depth_override_enabled());
        EXPECT_TRUE(material.is_dirty());
        EXPECT_FALSE(material.depth.is_test_enabled);
        EXPECT_FALSE(material.depth.is_write_enabled);
        EXPECT_TRUE(material.depth.is_prepass_enabled);
        EXPECT_EQ(material.depth.function, MaterialDepthFunction::Always);
    }

    // Validates that Material config owns material-level visibility and shadow settings.
    TEST(MaterialTests, MaterialConfig_Constructor_InitializesWithDefaults)
    {
        // Arrange
        Material material = {};

        // Act
        const bool is_two_sided = material.config.is_two_sided;
        const bool is_cullable = material.config.is_cullable;
        const auto shadow_mode = material.config.shadow_mode;

        // Assert
        EXPECT_FALSE(is_two_sided);
        EXPECT_TRUE(is_cullable);
        EXPECT_EQ(shadow_mode, ShadowMode::Standard);
    }
}
