#include "PCH.h"
#include "tbx/assets/builtin_assets.h"
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
        parameters.set("shininess_strength", 32.0f);

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
        EXPECT_EQ(texture_count, 5);
        EXPECT_NE(textures.begin(), textures.end());
    }

    // Validates PbrMaterialInstance writes directly to inherited bindings on set.
    TEST(RendererTests, PbrMaterialInstance_Setters_PopulateBindings)
    {
        // Arrange
        auto material = PbrMaterialInstance {};
        material.set_diffuse_map(Handle("Diffuse"));
        material.set_normal_map(Handle("Normal"));
        material.set_specular_map(Handle("Specular"));
        material.set_shininess_map(Handle("Shininess"));
        material.set_emissive_map(Handle("Emissive"));
        material.set_color(Color(0.25f, 0.5f, 0.75f, 1.0f));
        material.set_diffuse_strength(0.9f);
        material.set_normal_strength(0.8f);
        material.set_specular_strength(0.2f);
        material.set_shininess_strength(64.0f);
        material.set_emissive_color(Color(0.1f, 0.2f, 0.3f, 1.0f));
        material.set_emissive_strength(0.5f);
        material.set_alpha_cutoff(0.15f);
        material.set_transparency_amount(0.35f);
        material.set_exposure(1.5f);

        // Act
        const auto* diffuse_map = material.textures.get("diffuse_map");
        const auto* normal_map = material.textures.get("normal_map");
        const auto* specular_map = material.textures.get("specular_map");
        const auto* shininess_map = material.textures.get("shininess_map");
        const auto* emissive_map = material.textures.get("emissive_map");
        const auto* color = material.parameters.get("color");
        const auto* diffuse_strength = material.parameters.get("diffuse_strength");
        const auto* normal_strength = material.parameters.get("normal_strength");
        const auto* specular_strength = material.parameters.get("specular_strength");
        const auto* shininess_strength = material.parameters.get("shininess_strength");
        const auto* emissive = material.parameters.get("emissive");
        const auto* emissive_strength = material.parameters.get("emissive_strength");
        const auto* alpha_cutoff = material.parameters.get("alpha_cutoff");
        const auto* transparency_amount = material.parameters.get("transparency_amount");
        const auto* exposure = material.parameters.get("exposure");

        // Assert
        ASSERT_NE(diffuse_map, nullptr);
        ASSERT_NE(normal_map, nullptr);
        ASSERT_NE(specular_map, nullptr);
        ASSERT_NE(shininess_map, nullptr);
        ASSERT_NE(emissive_map, nullptr);
        ASSERT_NE(color, nullptr);
        ASSERT_NE(diffuse_strength, nullptr);
        ASSERT_NE(normal_strength, nullptr);
        ASSERT_NE(specular_strength, nullptr);
        ASSERT_NE(shininess_strength, nullptr);
        ASSERT_NE(emissive, nullptr);
        ASSERT_NE(emissive_strength, nullptr);
        ASSERT_NE(alpha_cutoff, nullptr);
        ASSERT_NE(transparency_amount, nullptr);
        ASSERT_NE(exposure, nullptr);
        EXPECT_EQ(diffuse_map->name, "u_diffuse_map");
        EXPECT_EQ(normal_map->name, "u_normal_map");
        EXPECT_EQ(specular_map->name, "u_specular_map");
        EXPECT_EQ(shininess_map->name, "u_shininess_map");
        EXPECT_EQ(emissive_map->name, "u_emissive_map");
    }

    // Validates PbrMaterialInstance default getters return expected fallback values.
    TEST(RendererTests, PbrMaterialInstance_DefaultGetters_ReturnExpectedDefaults)
    {
        // Arrange
        const auto material = PbrMaterialInstance {};

        // Act
        const auto color = material.get_color();
        const auto emissive = material.get_emissive_color();

        // Assert
        EXPECT_FLOAT_EQ(color.r, 1.0f);
        EXPECT_FLOAT_EQ(color.g, 1.0f);
        EXPECT_FLOAT_EQ(color.b, 1.0f);
        EXPECT_FLOAT_EQ(color.a, 1.0f);
        EXPECT_FLOAT_EQ(material.get_diffuse_strength(), 1.0f);
        EXPECT_FALSE(material.get_normal_map().is_valid());
        EXPECT_FLOAT_EQ(material.get_normal_strength(), 1.0f);
        EXPECT_FALSE(material.get_specular_map().is_valid());
        EXPECT_FLOAT_EQ(material.get_specular_strength(), 0.5f);
        EXPECT_FALSE(material.get_shininess_map().is_valid());
        EXPECT_FLOAT_EQ(material.get_shininess_strength(), 32.0f);
        EXPECT_FLOAT_EQ(emissive.r, 0.0f);
        EXPECT_FLOAT_EQ(emissive.g, 0.0f);
        EXPECT_FLOAT_EQ(emissive.b, 0.0f);
        EXPECT_FLOAT_EQ(emissive.a, 1.0f);
        EXPECT_FALSE(material.get_emissive_map().is_valid());
        EXPECT_FLOAT_EQ(material.get_emissive_strength(), 1.0f);
        EXPECT_FLOAT_EQ(material.get_alpha_cutoff(), 0.1f);
        EXPECT_FLOAT_EQ(material.get_transparency_amount(), 0.0f);
        EXPECT_FLOAT_EQ(material.get_exposure(), 1.0f);
    }

    // Validates Renderer can return a typed PBR material view from its stored MaterialInstance.
    TEST(RendererTests, Renderer_GetMaterial_ReturnsTypedPbrMaterial)
    {
        // Arrange
        auto renderer = Renderer {};
        renderer.material = PbrMaterialInstance(Color(0.2f, 0.4f, 0.6f, 1.0f));

        // Act
        const auto material = renderer.get_material<PbrMaterialInstance>();

        // Assert
        EXPECT_FLOAT_EQ(material.get_color().r, 0.2f);
        EXPECT_FLOAT_EQ(material.get_color().g, 0.4f);
        EXPECT_FLOAT_EQ(material.get_color().b, 0.6f);
    }

    // Validates Renderer can return a typed flat material view from its stored MaterialInstance.
    TEST(RendererTests, Renderer_GetMaterial_ReturnsTypedFlatMaterial)
    {
        // Arrange
        auto renderer = Renderer {};
        renderer.material = FlatMaterialInstance(Color(0.8f, 0.6f, 0.4f, 1.0f));

        // Act
        const auto material = renderer.get_material<FlatMaterialInstance>();

        // Assert
        EXPECT_EQ(material.handle.id, unlit_material.id);
        EXPECT_FLOAT_EQ(material.get_color().r, 0.8f);
        EXPECT_FLOAT_EQ(material.get_color().g, 0.6f);
        EXPECT_FLOAT_EQ(material.get_color().b, 0.4f);
    }

    // Validates FlatMaterialInstance writes directly to inherited bindings on set.
    TEST(RendererTests, FlatMaterialInstance_Setters_PopulateBindings)
    {
        // Arrange
        auto material = FlatMaterialInstance {};
        material.set_diffuse_map(Handle("Diffuse"));
        material.set_color(Color(0.2f, 0.4f, 0.6f, 1.0f));
        material.set_emissive_color(Color(0.1f, 0.2f, 0.3f, 1.0f));
        material.set_alpha_cutoff(0.2f);
        material.set_transparency_amount(0.45f);
        material.set_exposure(1.25f);

        // Act
        const auto* diffuse_map = material.textures.get("diffuse_map");
        const auto* color = material.parameters.get("color");
        const auto* emissive = material.parameters.get("emissive");
        const auto* alpha_cutoff = material.parameters.get("alpha_cutoff");
        const auto* transparency_amount = material.parameters.get("transparency_amount");
        const auto* exposure = material.parameters.get("exposure");

        // Assert
        EXPECT_EQ(material.handle.id, unlit_material.id);
        ASSERT_NE(diffuse_map, nullptr);
        ASSERT_NE(color, nullptr);
        ASSERT_NE(emissive, nullptr);
        ASSERT_NE(alpha_cutoff, nullptr);
        ASSERT_NE(transparency_amount, nullptr);
        ASSERT_NE(exposure, nullptr);
        EXPECT_EQ(diffuse_map->name, "u_diffuse_map");
    }

    // Validates FlatMaterialInstance default getters return expected fallback values.
    TEST(RendererTests, FlatMaterialInstance_DefaultGetters_ReturnExpectedDefaults)
    {
        // Arrange
        const auto material = FlatMaterialInstance {};

        // Act
        const auto color = material.get_color();
        const auto emissive = material.get_emissive_color();

        // Assert
        EXPECT_EQ(material.handle.id, unlit_material.id);
        EXPECT_FLOAT_EQ(color.r, 1.0f);
        EXPECT_FLOAT_EQ(color.g, 1.0f);
        EXPECT_FLOAT_EQ(color.b, 1.0f);
        EXPECT_FLOAT_EQ(color.a, 1.0f);
        EXPECT_FLOAT_EQ(emissive.r, 0.0f);
        EXPECT_FLOAT_EQ(emissive.g, 0.0f);
        EXPECT_FLOAT_EQ(emissive.b, 0.0f);
        EXPECT_FLOAT_EQ(emissive.a, 1.0f);
        EXPECT_FALSE(material.get_diffuse_map().is_valid());
        EXPECT_FLOAT_EQ(material.get_alpha_cutoff(), 0.1f);
        EXPECT_FLOAT_EQ(material.get_transparency_amount(), 0.0f);
        EXPECT_FLOAT_EQ(material.get_exposure(), 1.0f);
    }
}
