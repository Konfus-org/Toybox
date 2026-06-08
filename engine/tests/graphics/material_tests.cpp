#include "tbx/types/assets/builtin_assets.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/components/material_instance.h"
#include <utility>

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
        auto matrix3 = Mat3(1.0F);
        matrix3[2] = Vec3(4.0F, 5.0F, 1.0F);
        MaterialParameterData matrix3_data = matrix3;
        auto matrix = Mat4(1.0F);
        matrix[3] = Vec4(4.0F, 5.0F, 6.0F, 1.0F);
        MaterialParameterData matrix_data = matrix;

        // Act
        auto color_json = nlohmann::json();
        auto vector_json = nlohmann::json();
        auto matrix3_json = nlohmann::json();
        auto matrix_json = nlohmann::json();
        serialize_serializable_variant(color_json, color_data);
        serialize_serializable_variant(vector_json, vector_data);
        serialize_serializable_variant(matrix3_json, matrix3_data);
        serialize_serializable_variant(matrix_json, matrix_data);
        auto color_result = MaterialParameterData();
        auto vector_result = MaterialParameterData();
        auto matrix3_result = MaterialParameterData();
        auto matrix_result = MaterialParameterData();
        deserialize_serializable_variant(color_json, color_result);
        deserialize_serializable_variant(vector_json, vector_result);
        deserialize_serializable_variant(matrix3_json, matrix3_result);
        deserialize_serializable_variant(matrix_json, matrix_result);

        // Assert
        EXPECT_EQ(color_json.at("type").get<std::string>(), "color");
        EXPECT_EQ(vector_json.at("type").get<std::string>(), "vec3");
        EXPECT_EQ(matrix3_json.at("type").get<std::string>(), "mat3");
        EXPECT_EQ(matrix_json.at("type").get<std::string>(), "mat4");

        const auto& color = std::get<Color>(color_result);
        const auto& vector = std::get<Vec3>(vector_result);
        const auto& matrix3_value = std::get<Mat3>(matrix3_result);
        const auto& matrix_value = std::get<Mat4>(matrix_result);
        EXPECT_FLOAT_EQ(color.r, 0.25F);
        EXPECT_FLOAT_EQ(color.g, 0.5F);
        EXPECT_FLOAT_EQ(color.b, 0.75F);
        EXPECT_FLOAT_EQ(vector.x, 1.0F);
        EXPECT_FLOAT_EQ(vector.y, 2.0F);
        EXPECT_FLOAT_EQ(vector.z, 3.0F);
        EXPECT_FLOAT_EQ(matrix3_value[2].x, 4.0F);
        EXPECT_FLOAT_EQ(matrix3_value[2].y, 5.0F);
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
        auto parameter_json = nlohmann::json();
        auto texture_json = nlohmann::json();
        serialize(parameter_json, parameter);
        serialize(texture_json, texture);
        auto parameter_result = MaterialParameter();
        auto texture_result = MaterialTextureBinding();
        deserialize(parameter_json, parameter_result);
        deserialize(texture_json, texture_result);
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
        EXPECT_TRUE(texture_result.texture.name.empty());
        EXPECT_EQ(texture_result.texture.id, texture.texture.id);
        EXPECT_TRUE(parameters.has("albedo_color"));
        EXPECT_TRUE(textures.has("albedo_map"));
    }

    TEST(MaterialTests, MaterialBindings_GetByIdResolvesDeserializedNames)
    {
        // Arrange
        auto parameters = MaterialParameterBindings {};
        auto textures = MaterialTextureBindings {};
        auto parameter_value = MaterialParameter();
        parameter_value.name = "albedo_color";
        parameter_value.data = Color(0.25F, 0.5F, 0.75F, 1.0F);
        auto texture_value = MaterialTextureBinding();
        texture_value.name = "albedo_map";
        texture_value.texture = Handle("Textures/Smily.png");
        parameters.values.push_back(std::move(parameter_value));
        textures.values.push_back(std::move(texture_value));

        // Act
        const auto parameter = parameters.get(PbrMaterial::ALBEDO_COLOR);
        const auto texture = textures.get(PbrMaterial::ALBEDO_MAP);

        // Assert
        ASSERT_TRUE(parameter.has_value());
        ASSERT_TRUE(texture.has_value());
        EXPECT_EQ(parameter->get().id, INVALID_MATERIAL_PARAM_ID);
        EXPECT_EQ(texture->get().id, INVALID_MATERIAL_PARAM_ID);
        EXPECT_EQ(std::get<Color>(parameter->get().data).b, 0.75F);
        EXPECT_EQ(texture->get().texture.name, "Textures/Smily.png");
    }

    TEST(MaterialTests, MaterialBindings_GetByIdRejectsMismatchedDeserializedNames)
    {
        // Arrange
        auto parameters = MaterialParameterBindings {};
        auto textures = MaterialTextureBindings {};
        auto parameter_value = MaterialParameter();
        parameter_value.name = "emissive_color";
        parameter_value.data = Color(0.25F, 0.5F, 0.75F, 1.0F);
        auto texture_value = MaterialTextureBinding();
        texture_value.name = "normal_map";
        texture_value.texture = Handle("Textures/Normal.png");
        parameters.values.push_back(std::move(parameter_value));
        textures.values.push_back(std::move(texture_value));

        // Act
        const auto parameter = parameters.get(PbrMaterial::ALBEDO_COLOR);
        const auto texture = textures.get(PbrMaterial::ALBEDO_MAP);

        // Assert
        EXPECT_FALSE(parameter.has_value());
        EXPECT_FALSE(texture.has_value());
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
        EXPECT_EQ(shadow_mode, ShadowMode::ON);
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

    TEST(MaterialTests, MaterialParameter_StoresDeclaredUploadTarget)
    {
        // Arrange
        auto parameter = MaterialParameter("albedo_color", Color::WHITE);

        // Act
        parameter.target = MaterialBindingTarget::BASE_COLOR;

        // Assert
        EXPECT_EQ(parameter.name, "albedo_color");
        EXPECT_EQ(parameter.target, MaterialBindingTarget::BASE_COLOR);
        EXPECT_TRUE(std::holds_alternative<Color>(parameter.data));
    }

    TEST(MaterialTests, MaterialTextureBinding_StoresDeclaredUploadTarget)
    {
        // Arrange
        auto texture = MaterialTextureBinding("albedo_map", Handle("Textures/Smily.png"));

        // Act
        texture.target = MaterialBindingTarget::ALBEDO_TEXTURE;

        // Assert
        EXPECT_EQ(texture.name, "albedo_map");
        EXPECT_EQ(texture.target, MaterialBindingTarget::ALBEDO_TEXTURE);
        EXPECT_EQ(texture.texture.name, "Textures/Smily.png");
    }
}
