#include "PCH.h"
#include "tbx/core/systems/assets/builtin_assets.h"
#include "tbx/core/types/handle.h"
#include "tbx/core/systems/graphics/material.h"

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
        EXPECT_EQ(material.get_handle().get_id(), PbrMaterial::HANDLE.get_id());
        EXPECT_FLOAT_EQ(color.r, 0.25f);
        EXPECT_FLOAT_EQ(color.g, 0.5f);
        EXPECT_FLOAT_EQ(color.b, 0.75f);
        EXPECT_FLOAT_EQ(diffuse_strength, 0.9f);
        EXPECT_FLOAT_EQ(emissive_strength, 0.5f);
        EXPECT_EQ(diffuse_map.get_id(), CheckerboardTexture::HANDLE.get_id());
        EXPECT_EQ(normal_map.get_name(), "Textures/NeutralNormal.png");
    }

    // Validates that config overrides are tracked separately from material asset config.
    TEST(MaterialTests, MaterialInstance_SetConfig_EnablesConfigOverride)
    {
        // Arrange
        auto material = MaterialInstance(PbrMaterial::HANDLE);
        material.clear_dirty();

        // Act
        material.set_config(MaterialConfig {
            .is_depth_test_enabled = false,
            .is_depth_write_enabled = false,
            .is_depth_prepass_enabled = true,
            .depth_function = MaterialDepthFunction::Always,
        });

        // Assert
        EXPECT_TRUE(material.has_config_override_enabled());
        EXPECT_TRUE(material.is_dirty());
        EXPECT_FALSE(material.config.is_depth_test_enabled);
        EXPECT_FALSE(material.config.is_depth_write_enabled);
        EXPECT_TRUE(material.config.is_depth_prepass_enabled);
        EXPECT_EQ(material.config.depth_function, MaterialDepthFunction::Always);
    }

    // Validates that Material config owns material render state.
    TEST(MaterialTests, MaterialConfig_Constructor_InitializesWithDefaults)
    {
        // Arrange
        Material material = {};

        // Act
        const bool is_depth_test_enabled = material.config.is_depth_test_enabled;
        const bool is_depth_write_enabled = material.config.is_depth_write_enabled;
        const bool is_depth_prepass_enabled = material.config.is_depth_prepass_enabled;
        const auto depth_function = material.config.depth_function;
        const auto blend_mode = material.config.blend_mode;
        const bool is_two_sided = material.config.is_two_sided;
        const bool is_cullable = material.config.is_cullable;
        const auto shadow_mode = material.config.shadow_mode;

        // Assert
        EXPECT_TRUE(is_depth_test_enabled);
        EXPECT_TRUE(is_depth_write_enabled);
        EXPECT_FALSE(is_depth_prepass_enabled);
        EXPECT_EQ(depth_function, MaterialDepthFunction::Less);
        EXPECT_EQ(blend_mode, MaterialBlendMode::Opaque);
        EXPECT_FALSE(is_two_sided);
        EXPECT_TRUE(is_cullable);
        EXPECT_EQ(shadow_mode, ShadowMode::Standard);
    }
}
