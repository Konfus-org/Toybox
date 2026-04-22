#include "PCH.h"
#include "tbx/core/systems/assets/builtin_assets.h"
#include "tbx/core/systems/graphics/post_processing.h"
#include "tbx/core/systems/graphics/renderer.h"

namespace tbx::tests::graphics
{
    // Validates that Sky defaults to an unset material handle.
    TEST(RendererTests, Sky_Constructor_InitializesWithDefaults)
    {
        // Arrange
        Sky sky = {};

        // Act
        const bool is_material_valid = sky.material.get_handle().is_valid();

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
        const bool is_material_valid = effect.material.get_handle().is_valid();
        const bool is_enabled = effect.is_enabled;
        const float blend = effect.blend;

        // Assert
        EXPECT_FALSE(is_material_valid);
        EXPECT_TRUE(is_enabled);
        EXPECT_FLOAT_EQ(blend, 1.0f);
    }

    // Validates that MaterialInstance defaults to an unset material and empty override sets.
    TEST(RendererTests, MaterialInstance_Constructor_InitializesWithDefaults)
    {
        // Arrange
        MaterialInstance material_instance = {};

        // Act
        const bool is_material_valid = material_instance.get_handle().is_valid();
        const bool has_parameters = !material_instance.param_overrides.values.empty();
        const bool has_textures = !material_instance.texture_overrides.values.empty();

        // Assert
        EXPECT_FALSE(is_material_valid);
        EXPECT_FALSE(has_parameters);
        EXPECT_FALSE(has_textures);
    }

    // Validates that MaterialInstance can be created with a fixed material handle.
    TEST(RendererTests, MaterialInstance_HandleCtor_InitializesWithProvidedHandle)
    {
        // Arrange
        auto material_instance = MaterialInstance(FlatMaterial::HANDLE);

        // Act
        const auto& handle = material_instance.get_handle();

        // Assert
        EXPECT_EQ(handle.get_id(), FlatMaterial::HANDLE.get_id());
    }

    // Validates that material mutations toggle the dirty bit used by GPU upload paths.
    TEST(RendererTests, MaterialInstance_Mutations_MarkDirty)
    {
        // Arrange
        auto material_instance = MaterialInstance(FlatMaterial::HANDLE);
        material_instance.clear_dirty();

        // Act
        material_instance.set_parameter(tbx::FlatMaterial::COLOR, Color(1.0f, 0.5f, 0.25f, 1.0f));
        const auto dirty_after_parameter_set = material_instance.is_dirty();
        material_instance.clear_dirty();
        material_instance.set_texture(tbx::FlatMaterial::DIFFUSE_MAP, Handle("Diffuse"));
        const auto dirty_after_texture_set = material_instance.is_dirty();

        // Assert
        EXPECT_TRUE(dirty_after_parameter_set);
        EXPECT_TRUE(dirty_after_texture_set);
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

    // Validates generated material keys expose stable handles and binding names.
    TEST(RendererTests, MaterialKeys_ExposeTypedHandlesAndNames)
    {
        // Arrange
        auto material = MaterialInstance(PbrMaterial::HANDLE);
        material.set_parameter(PbrMaterial::COLOR, Color::RED);
        material.set_texture(PbrMaterial::DIFFUSE_MAP, CheckerboardTexture::HANDLE);

        // Act
        const auto color = material.get_parameter_or(PbrMaterial::COLOR, Color::BLACK);
        const auto diffuse_map = material.get_texture_handle_or(PbrMaterial::DIFFUSE_MAP);

        // Assert
        EXPECT_TRUE(PbrMaterial::HANDLE.is_valid());
        EXPECT_TRUE(FlatMaterial::HANDLE.is_valid());
        EXPECT_FLOAT_EQ(color.r, Color::RED.r);
        EXPECT_FLOAT_EQ(color.g, Color::RED.g);
        EXPECT_FLOAT_EQ(color.b, Color::RED.b);
        EXPECT_FLOAT_EQ(color.a, Color::RED.a);
        EXPECT_EQ(diffuse_map.get_id(), CheckerboardTexture::HANDLE.get_id());
    }

    // Validates Lods defaults to no LOD entries and no render-distance cap.
    TEST(RendererTests, Lods_Constructor_InitializesWithDefaults)
    {
        // Arrange
        Lods lods = {};

        // Act
        const bool has_values = !lods.values.empty();
        const float render_distance = lods.render_distance;

        // Assert
        EXPECT_FALSE(has_values);
        EXPECT_FLOAT_EQ(render_distance, 0.0f);
    }
}
