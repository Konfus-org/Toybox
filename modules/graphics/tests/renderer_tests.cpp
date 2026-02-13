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
        parameters.set("color", RgbaColor(1.0f, 0.5f, 0.25f, 1.0f));
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
}
