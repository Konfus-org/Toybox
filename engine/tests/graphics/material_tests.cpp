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
        EXPECT_EQ(material.get_handle().id, PbrMaterial::HANDLE.id);
        EXPECT_TRUE(material.overrides.has_parameter_override);
        EXPECT_TRUE(material.overrides.has_texture_override);
        EXPECT_FLOAT_EQ(color.r, 0.25f);
        EXPECT_FLOAT_EQ(color.g, 0.5f);
        EXPECT_FLOAT_EQ(color.b, 0.75f);
        EXPECT_FLOAT_EQ(emissive.r, 0.5f);
        EXPECT_FLOAT_EQ(roughness, 0.9f);
        EXPECT_EQ(diffuse_map.id, CheckerboardTexture::HANDLE.id);
        EXPECT_EQ(normal_map.name, "Textures/NeutralNormal.png");
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
        EXPECT_EQ(material.get_handle().id, TexturedSkyMaterial::HANDLE.id);
        EXPECT_TRUE(material.overrides.has_parameter_override);
        EXPECT_TRUE(material.overrides.has_texture_override);
        EXPECT_EQ(texture.name, "Textures/DaySky.png");
        EXPECT_EQ(secondary_texture.name, "Textures/NightSky.png");
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

    // Validates that material parameter variants delegate value serialization to registered types.
    TEST(MaterialTests, MaterialParameterDataSerialization_RoundTripsSerializableValues)
    {
        // Arrange
        MaterialParameterData color_data = Color(0.25F, 0.5F, 0.75F, 1.0F);
        MaterialParameterData vector_data = Vec3(1.0F, 2.0F, 3.0F);
        auto matrix = Mat4(1.0F);
        matrix[3] = Vec4(4.0F, 5.0F, 6.0F, 1.0F);
        MaterialParameterData matrix_data = matrix;

        // Act
        const nlohmann::json color_json = color_data;
        const nlohmann::json vector_json = vector_data;
        const nlohmann::json matrix_json = matrix_data;
        const auto color_result = color_json.get<MaterialParameterData>();
        const auto vector_result = vector_json.get<MaterialParameterData>();
        const auto matrix_result = matrix_json.get<MaterialParameterData>();

        // Assert
        EXPECT_EQ(color_json.at("type").get<std::string>(), "color");
        EXPECT_EQ(vector_json.at("type").get<std::string>(), "vec3");
        EXPECT_EQ(matrix_json.at("type").get<std::string>(), "mat4");

        const auto& color = std::get<Color>(color_result);
        const auto& vector = std::get<Vec3>(vector_result);
        const auto& matrix_value = std::get<Mat4>(matrix_result);
        EXPECT_FLOAT_EQ(color.r, 0.25F);
        EXPECT_FLOAT_EQ(color.g, 0.5F);
        EXPECT_FLOAT_EQ(color.b, 0.75F);
        EXPECT_FLOAT_EQ(vector.x, 1.0F);
        EXPECT_FLOAT_EQ(vector.y, 2.0F);
        EXPECT_FLOAT_EQ(vector.z, 3.0F);
        EXPECT_FLOAT_EQ(matrix_value[3].x, 4.0F);
        EXPECT_FLOAT_EQ(matrix_value[3].y, 5.0F);
        EXPECT_FLOAT_EQ(matrix_value[3].z, 6.0F);
    }

    TEST(MaterialTests, MaterialBindingSerialization_UsesNamesAsAuthoredKeys)
    {
        // Arrange
        const auto parameter =
            MaterialParameter(std::string_view("albedo_color"), Color(0.25F, 0.5F, 0.75F, 1.0F));
        const auto texture =
            MaterialTextureBinding(std::string_view("albedo_map"), Handle("Textures/Smily.png"));

        // Act
        const nlohmann::json parameter_json = parameter;
        const nlohmann::json texture_json = texture;
        const auto parameter_result = parameter_json.get<MaterialParameter>();
        const auto texture_result = texture_json.get<MaterialTextureBinding>();
        auto parameters = MaterialParameterBindings {};
        auto textures = MaterialTextureBindings {};
        parameters.set(parameter_result);
        textures.set(texture_result);

        // Assert
        EXPECT_FALSE(parameter_json.contains("id"));
        EXPECT_FALSE(texture_json.contains("id"));
        EXPECT_EQ(parameter_result.name, "albedo_color");
        EXPECT_EQ(parameter_result.id, INVALID_MATERIAL_PARAM_ID);
        EXPECT_TRUE(std::holds_alternative<Color>(parameter_result.data));
        EXPECT_EQ(texture_result.name, "albedo_map");
        EXPECT_EQ(texture_result.id, INVALID_MATERIAL_PARAM_ID);
        EXPECT_EQ(texture_result.texture.name, "Textures/Smily.png");
        EXPECT_TRUE(parameters.has("albedo_color"));
        EXPECT_TRUE(textures.has("albedo_map"));
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

    TEST(MaterialTests, StdHash_ChangesForMaterialInputs)
    {
        // Arrange
        auto first_config = MaterialConfig {};
        auto second_config = first_config;
        second_config.is_two_sided = true;

        auto first_parameter = MaterialParameterData(1.0F);
        auto second_parameter = MaterialParameterData(Vec4(1.0F, 0.0F, 0.0F, 1.0F));

        auto first_material = MaterialInstance(Handle("Materials/One.mat", Uuid(0x10U)));
        auto second_material = first_material;
        second_material.set_texture(PbrMaterial::ALBEDO_MAP, Handle("Textures/Other.png"));

        // Act
        const auto first_config_hash = std::hash<MaterialConfig>()(first_config);
        const auto second_config_hash = std::hash<MaterialConfig>()(second_config);
        const auto first_parameter_hash = std::hash<MaterialParameterData>()(first_parameter);
        const auto second_parameter_hash = std::hash<MaterialParameterData>()(second_parameter);
        const auto first_material_hash = std::hash<MaterialInstance>()(first_material);
        const auto second_material_hash = std::hash<MaterialInstance>()(second_material);

        // Assert
        EXPECT_NE(first_config_hash, second_config_hash);
        EXPECT_NE(first_parameter_hash, second_parameter_hash);
        EXPECT_NE(first_material_hash, second_material_hash);
    }
}
