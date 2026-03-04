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
        parameters.set("roughness", 0.25f);

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
        textures.set("diffuse", Handle("Diffuse"));
        textures.set("normal", Handle("Normal"));

        // Act
        const auto* diffuse = textures.get("diffuse");
        int texture_count = 0;
        for (const auto& texture_binding : textures)
        {
            (void)texture_binding;
            ++texture_count;
        }

        // Assert
        ASSERT_NE(diffuse, nullptr);
        EXPECT_EQ(diffuse->name, "u_diffuse");
        EXPECT_EQ(texture_count, 2);
        EXPECT_NE(textures.begin(), textures.end());
    }

    // Validates StandardMaterialInstance writes directly to inherited bindings on set.
    TEST(RendererTests, StandardMaterialInstance_Setters_PopulateBindings)
    {
        // Arrange
        auto material = StandardMaterialInstance {};
        material.set_diffuse(Handle("Diffuse"));
        material.set_normal(Handle("Normal"));
        material.set_color(Color(0.25f, 0.5f, 0.75f, 1.0f));
        material.set_metallic(0.2f);
        material.set_roughness(0.8f);
        material.set_emissive(Color(0.1f, 0.2f, 0.3f, 1.0f));
        material.set_occlusion(0.6f);
        material.set_alpha_cutoff(0.15f);
        material.set_exposure(1.5f);
        material.set_unlit(true);

        // Act
        const auto* diffuse = material.textures.get("diffuse");
        const auto* normal = material.textures.get("normal");
        const auto* color = material.parameters.get("color");
        const auto* metallic = material.parameters.get("metallic");
        const auto* roughness = material.parameters.get("roughness");
        const auto* emissive = material.parameters.get("emissive");
        const auto* occlusion = material.parameters.get("occlusion");
        const auto* alpha_cutoff = material.parameters.get("alpha_cutoff");
        const auto* exposure = material.parameters.get("exposure");
        const auto* unlit = material.parameters.get("unlit");

        // Assert
        ASSERT_NE(diffuse, nullptr);
        ASSERT_NE(normal, nullptr);
        ASSERT_NE(color, nullptr);
        ASSERT_NE(metallic, nullptr);
        ASSERT_NE(roughness, nullptr);
        ASSERT_NE(emissive, nullptr);
        ASSERT_NE(occlusion, nullptr);
        ASSERT_NE(alpha_cutoff, nullptr);
        ASSERT_NE(exposure, nullptr);
        ASSERT_NE(unlit, nullptr);
        EXPECT_EQ(diffuse->name, "u_diffuse");
        EXPECT_EQ(normal->name, "u_normal");
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
        EXPECT_FLOAT_EQ(material.get_metallic(), 0.0f);
        EXPECT_FLOAT_EQ(material.get_roughness(), 1.0f);
        EXPECT_FLOAT_EQ(emissive.r, 0.0f);
        EXPECT_FLOAT_EQ(emissive.g, 0.0f);
        EXPECT_FLOAT_EQ(emissive.b, 0.0f);
        EXPECT_FLOAT_EQ(emissive.a, 1.0f);
        EXPECT_FLOAT_EQ(material.get_occlusion(), 1.0f);
        EXPECT_FLOAT_EQ(material.get_alpha_cutoff(), 0.1f);
        EXPECT_FLOAT_EQ(material.get_exposure(), 1.0f);
        EXPECT_FALSE(material.get_unlit());
    }
}
