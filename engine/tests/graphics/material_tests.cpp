#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/types/handle.h"
#include "tbx/types/material.h"

namespace tbx::tests::graphics
{
    // Validates that a PBR material instance stores overrides using generated key constants.
    TEST(MaterialTests, PbrMaterialInstance_Overrides_AreStoredByGeneratedKeys)
    {
        // Arrange
        auto material = MaterialInstance(PbrMaterial::HANDLE);
        material.set_parameter(PbrMaterial::ALBEDO_COLOR, Color(0.25f, 0.5f, 0.75f, 1.0f));
        material.set_parameter(PbrMaterial::EMISSIVE_COLOR, Color(0.5f, 0.25f, 0.1f, 1.0f));
        material.set_parameter(PbrMaterial::METALLIC, 0.5f);
        material.set_parameter(PbrMaterial::ROUGHNESS, 0.9f);
        material.set_parameter(PbrMaterial::NORMAL_STRENGTH, 1.0f);
        material.set_parameter(PbrMaterial::AO, 1.0f);
        material.set_texture(PbrMaterial::ALBEDO_MAP, CheckerboardTexture::HANDLE);
        material.set_texture(PbrMaterial::NORMAL_MAP, Handle("Textures/NeutralNormal.png"));

        // Act
        const auto color = material.get_parameter_or(PbrMaterial::ALBEDO_COLOR, Color::BLACK);
        const auto emissive = material.get_parameter_or(PbrMaterial::EMISSIVE_COLOR, Color::BLACK);
        const auto roughness = material.get_parameter_or(PbrMaterial::ROUGHNESS, 0.0f);
        const auto diffuse_map = material.get_texture_handle_or(PbrMaterial::ALBEDO_MAP);
        const auto normal_map = material.get_texture_handle_or(PbrMaterial::NORMAL_MAP);

        // Assert
        EXPECT_EQ(material.get_handle().get_id(), PbrMaterial::HANDLE.get_id());
        EXPECT_TRUE(material.overrides.has_parameter_override);
        EXPECT_TRUE(material.overrides.has_texture_override);
        EXPECT_FLOAT_EQ(color.r, 0.25f);
        EXPECT_FLOAT_EQ(color.g, 0.5f);
        EXPECT_FLOAT_EQ(color.b, 0.75f);
        EXPECT_FLOAT_EQ(emissive.r, 0.5f);
        EXPECT_FLOAT_EQ(roughness, 0.9f);
        EXPECT_EQ(diffuse_map.get_id(), CheckerboardTexture::HANDLE.get_id());
        EXPECT_EQ(normal_map.get_name(), "Textures/NeutralNormal.png");
    }

    // Validates that sky material overrides use generated texture and color keys.
    TEST(MaterialTests, TexturedSkyMaterialInstance_Overrides_AreStoredByGeneratedKeys)
    {
        // Arrange
        auto material = MaterialInstance(TexturedSkyMaterial::HANDLE);
        material.set_texture(TexturedSkyMaterial::SKYBOX_TEXTURE, Handle("Textures/DaySky.png"));
        material.set_texture(
            TexturedSkyMaterial::SECONDARY_SKYBOX_TEXTURE,
            Handle("Textures/NightSky.png"));
        material.set_parameter(TexturedSkyMaterial::COLOR, Color(0.75F, 0.8F, 1.0F, 1.0F));
        material.set_parameter(TexturedSkyMaterial::BRIGHTNESS, 1.25F);

        // Act
        const auto texture = material.get_texture_handle_or(TexturedSkyMaterial::SKYBOX_TEXTURE);
        const auto secondary_texture =
            material.get_texture_handle_or(TexturedSkyMaterial::SECONDARY_SKYBOX_TEXTURE);
        const auto color = material.get_parameter_or(TexturedSkyMaterial::COLOR, Color::BLACK);
        const auto brightness = material.get_parameter_or(TexturedSkyMaterial::BRIGHTNESS, 0.0F);

        // Assert
        EXPECT_EQ(material.get_handle().get_id(), TexturedSkyMaterial::HANDLE.get_id());
        EXPECT_TRUE(material.overrides.has_parameter_override);
        EXPECT_TRUE(material.overrides.has_texture_override);
        EXPECT_EQ(texture.get_name(), "Textures/DaySky.png");
        EXPECT_EQ(secondary_texture.get_name(), "Textures/NightSky.png");
        EXPECT_FLOAT_EQ(color.r, 0.75F);
        EXPECT_FLOAT_EQ(brightness, 1.25F);
    }

    // Validates that config overrides are tracked separately from material asset config.
    TEST(MaterialTests, MaterialInstance_SetConfig_EnablesConfigOverride)
    {
        // Arrange
        auto material = MaterialInstance(PbrMaterial::HANDLE);
        material.clear_dirty();

        // Act
        material.set_config(
            MaterialConfig {
                .is_depth_test_enabled = false,
                .is_depth_write_enabled = false,
                .is_depth_prepass_enabled = true,
                .depth_function = MaterialDepthFunction::ALWAYS,
            });

        // Assert
        EXPECT_TRUE(material.has_config_override_enabled());
        EXPECT_TRUE(material.overrides.has_config_override);
        EXPECT_TRUE(material.is_dirty());
        EXPECT_FALSE(material.overrides.config.is_depth_test_enabled);
        EXPECT_FALSE(material.overrides.config.is_depth_write_enabled);
        EXPECT_TRUE(material.overrides.config.is_depth_prepass_enabled);
        EXPECT_EQ(material.overrides.config.depth_function, MaterialDepthFunction::ALWAYS);
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
        EXPECT_EQ(depth_function, MaterialDepthFunction::LESS);
        EXPECT_EQ(blend_mode, MaterialBlendMode::OPAQUE);
        EXPECT_FALSE(is_two_sided);
        EXPECT_TRUE(is_cullable);
        EXPECT_EQ(shadow_mode, ShadowMode::STANDARD);
    }
}
