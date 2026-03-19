#include "PCH.h"
#include "tbx/graphics/renderer.h"

namespace tbx::tests::graphics
{
    // Validates that Sky defaults to an unset material handle.
    TEST(RendererTests, Sky_Constructor_InitializesWithDefaults)
    {
        // Arrange
        Sky sky = {};

        // Act
        const bool is_material_valid = sky.material.handle.is_valid();

        // Assert
        EXPECT_FALSE(is_material_valid);
    }

    // Validates that PostProcessing defaults to enabled with an empty effect stack.
    TEST(RendererTests, PostProcessing_Constructor_InitializesWithDefaults)
    {
        // Arrange
        PostProcessing post_processing = {};

        // Act
        const bool is_enabled = post_processing.is_enabled;
        const bool has_effects = !post_processing.effects.empty();

        // Assert
        EXPECT_TRUE(is_enabled);
        EXPECT_FALSE(has_effects);
    }

    // Validates that PostProcessingEffect defaults to an enabled pass with full blend.
    TEST(RendererTests, PostProcessingEffect_Constructor_InitializesWithDefaults)
    {
        // Arrange
        PostProcessingEffect effect = {};

        // Act
        const bool is_material_valid = effect.material.handle.is_valid();
        const bool is_enabled = effect.is_enabled;
        const float blend = effect.blend;

        // Assert
        EXPECT_FALSE(is_material_valid);
        EXPECT_TRUE(is_enabled);
        EXPECT_FLOAT_EQ(blend, 1.0f);
    }

    // Validates that MaterialInstance defaults to an unset base material and empty values.
    TEST(RendererTests, MaterialInstance_Constructor_InitializesWithDefaults)
    {
        // Arrange
        MaterialInstance material_instance = {};

        // Act
        const bool is_base_material_valid = material_instance.handle.is_valid();
        const bool has_parameters = !material_instance.parameters.values.empty();
        const bool has_textures = !material_instance.textures.values.empty();

        // Assert
        EXPECT_FALSE(is_base_material_valid);
        EXPECT_FALSE(has_parameters);
        EXPECT_FALSE(has_textures);
    }

    // Validates MaterialParameterBindings supports normalized lookup and iteration.
    TEST(RendererTests, MaterialParameterBindings_GetAndIterate_WorkAsExpected)
    {
        // Arrange
        auto parameters = MaterialParameterBindings {};
        parameters.set("color", Color(1.0f, 0.5f, 0.25f, 1.0f));
        parameters.set("shininess", 32.0f);

        // Act
        const auto* color = parameters.get("color");
        int parameter_count = 0;
        for (const auto& parameter : parameters)
        {
            (void)parameter;
            ++parameter_count;
        }

        // Assert
        ASSERT_NE(color, nullptr);
        EXPECT_EQ(color->name, "u_color");
        EXPECT_EQ(parameter_count, 2);
        EXPECT_NE(parameters.begin(), parameters.end());
    }

    // Validates MaterialTextureBindings supports normalized lookup and iteration.
    TEST(RendererTests, MaterialTextureBindings_GetAndIterate_WorkAsExpected)
    {
        // Arrange
        auto textures = MaterialTextureBindings {};
        textures.set("diffuse_map", Handle("Diffuse"));
        textures.set("normal_map", Handle("Normal"));
        textures.set("specular_map", Handle("Specular"));
        textures.set("shininess_map", Handle("Shininess"));
        textures.set("emissive_map", Handle("Emissive"));
        textures.set("occlusion_map", Handle("Occlusion"));

        // Act
        const auto* diffuse_map = textures.get("diffuse_map");
        int texture_count = 0;
        for (const auto& texture_binding : textures)
        {
            (void)texture_binding;
            ++texture_count;
        }

        // Assert
        ASSERT_NE(diffuse_map, nullptr);
        EXPECT_EQ(diffuse_map->name, "u_diffuse_map");
        EXPECT_EQ(texture_count, 6);
        EXPECT_NE(textures.begin(), textures.end());
    }

    // Validates StandardMaterialInstance writes directly to inherited bindings on set.
    TEST(RendererTests, StandardMaterialInstance_Setters_PopulateBindings)
    {
        // Arrange
        auto material = StandardMaterialInstance {};
        material.set_diffuse_map(Handle("Diffuse"));
        material.set_normal_map(Handle("Normal"));
        material.set_specular_map(Handle("Specular"));
        material.set_shininess_map(Handle("Shininess"));
        material.set_emissive_map(Handle("Emissive"));
        material.set_occlusion_map(Handle("Occlusion"));
        material.set_color(Color(0.25f, 0.5f, 0.75f, 1.0f));
        material.set_diffuse_map_strength(0.9f);
        material.set_normal_map_strength(0.8f);
        material.set_specular(0.2f);
        material.set_specular_map_strength(0.7f);
        material.set_shininess(64.0f);
        material.set_shininess_map_strength(0.6f);
        material.set_emissive(Color(0.1f, 0.2f, 0.3f, 1.0f));
        material.set_emissive_map_strength(0.5f);
        material.set_occlusion(0.6f);
        material.set_occlusion_map_strength(0.4f);
        material.set_alpha_cutoff(0.15f);
        material.set_transparency_amount(0.35f);
        material.set_exposure(1.5f);

        // Act
        const auto* diffuse_map = material.textures.get("diffuse_map");
        const auto* normal_map = material.textures.get("normal_map");
        const auto* specular_map = material.textures.get("specular_map");
        const auto* shininess_map = material.textures.get("shininess_map");
        const auto* emissive_map = material.textures.get("emissive_map");
        const auto* occlusion_map = material.textures.get("occlusion_map");
        const auto* color = material.parameters.get("color");
        const auto* diffuse_map_strength = material.parameters.get("diffuse_map_strength");
        const auto* normal_map_strength = material.parameters.get("normal_map_strength");
        const auto* specular = material.parameters.get("specular");
        const auto* specular_map_strength = material.parameters.get("specular_map_strength");
        const auto* shininess = material.parameters.get("shininess");
        const auto* shininess_map_strength = material.parameters.get("shininess_map_strength");
        const auto* emissive = material.parameters.get("emissive");
        const auto* emissive_map_strength = material.parameters.get("emissive_map_strength");
        const auto* occlusion = material.parameters.get("occlusion");
        const auto* occlusion_map_strength = material.parameters.get("occlusion_map_strength");
        const auto* alpha_cutoff = material.parameters.get("alpha_cutoff");
        const auto* transparency_amount = material.parameters.get("transparency_amount");
        const auto* exposure = material.parameters.get("exposure");

        // Assert
        ASSERT_NE(diffuse_map, nullptr);
        ASSERT_NE(normal_map, nullptr);
        ASSERT_NE(specular_map, nullptr);
        ASSERT_NE(shininess_map, nullptr);
        ASSERT_NE(emissive_map, nullptr);
        ASSERT_NE(occlusion_map, nullptr);
        ASSERT_NE(color, nullptr);
        ASSERT_NE(diffuse_map_strength, nullptr);
        ASSERT_NE(normal_map_strength, nullptr);
        ASSERT_NE(specular, nullptr);
        ASSERT_NE(specular_map_strength, nullptr);
        ASSERT_NE(shininess, nullptr);
        ASSERT_NE(shininess_map_strength, nullptr);
        ASSERT_NE(emissive, nullptr);
        ASSERT_NE(emissive_map_strength, nullptr);
        ASSERT_NE(occlusion, nullptr);
        ASSERT_NE(occlusion_map_strength, nullptr);
        ASSERT_NE(alpha_cutoff, nullptr);
        ASSERT_NE(transparency_amount, nullptr);
        ASSERT_NE(exposure, nullptr);
        EXPECT_EQ(diffuse_map->name, "u_diffuse_map");
        EXPECT_EQ(normal_map->name, "u_normal_map");
        EXPECT_EQ(specular_map->name, "u_specular_map");
        EXPECT_EQ(shininess_map->name, "u_shininess_map");
        EXPECT_EQ(emissive_map->name, "u_emissive_map");
        EXPECT_EQ(occlusion_map->name, "u_occlusion_map");
    }

    // Validates StandardMaterialInstance default getters return expected fallback values.
    TEST(RendererTests, StandardMaterialInstance_DefaultGetters_ReturnExpectedDefaults)
    {
        // Arrange
        const auto material = StandardMaterialInstance {};

        // Act
        const auto color = material.get_color();
        const auto emissive = material.get_emissive();

        // Assert
        EXPECT_FLOAT_EQ(color.r, 1.0f);
        EXPECT_FLOAT_EQ(color.g, 1.0f);
        EXPECT_FLOAT_EQ(color.b, 1.0f);
        EXPECT_FLOAT_EQ(color.a, 1.0f);
        EXPECT_FLOAT_EQ(material.get_diffuse_map_strength(), 1.0f);
        EXPECT_FALSE(material.get_normal_map().is_valid());
        EXPECT_FLOAT_EQ(material.get_normal_map_strength(), 1.0f);
        EXPECT_FALSE(material.get_specular_map().is_valid());
        EXPECT_FLOAT_EQ(material.get_specular(), 0.5f);
        EXPECT_FLOAT_EQ(material.get_specular_map_strength(), 1.0f);
        EXPECT_FALSE(material.get_shininess_map().is_valid());
        EXPECT_FLOAT_EQ(material.get_shininess(), 32.0f);
        EXPECT_FLOAT_EQ(material.get_shininess_map_strength(), 1.0f);
        EXPECT_FLOAT_EQ(emissive.r, 0.0f);
        EXPECT_FLOAT_EQ(emissive.g, 0.0f);
        EXPECT_FLOAT_EQ(emissive.b, 0.0f);
        EXPECT_FLOAT_EQ(emissive.a, 1.0f);
        EXPECT_FALSE(material.get_emissive_map().is_valid());
        EXPECT_FLOAT_EQ(material.get_emissive_map_strength(), 1.0f);
        EXPECT_FLOAT_EQ(material.get_occlusion(), 1.0f);
        EXPECT_FALSE(material.get_occlusion_map().is_valid());
        EXPECT_FLOAT_EQ(material.get_occlusion_map_strength(), 1.0f);
        EXPECT_FLOAT_EQ(material.get_alpha_cutoff(), 0.1f);
        EXPECT_FLOAT_EQ(material.get_transparency_amount(), 0.0f);
        EXPECT_FLOAT_EQ(material.get_exposure(), 1.0f);
    }
}
