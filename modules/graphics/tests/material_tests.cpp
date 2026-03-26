#include "PCH.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/common/handle.h"
#include "tbx/graphics/material.h"

namespace tbx::tests::graphics
{
    // Validates that converting from a generic MaterialInstance preserves default hydration state.
    TEST(MaterialTests, PbrMaterialInstance_CopyCtor_PreservesLoadedDefaultsState)
    {
        // Arrange
        auto source = MaterialInstance();
        source.handle = Handle(Uuid(0x00000002U));
        source.has_loaded_defaults = true;
        source.parameters.set("color", Color(0.25f, 0.5f, 0.75f, 1.0f));
        source.parameters.set("diffuse_strength", 0.9f);
        source.parameters.set("normal_strength", 0.7f);
        source.parameters.set("specular_strength", 0.4f);
        source.parameters.set("shininess_strength", 48.0f);
        source.parameters.set("emissive", Color(0.1f, 0.2f, 0.3f, 1.0f));
        source.parameters.set("emissive_strength", 0.5f);
        source.parameters.set("alpha_cutoff", 0.3f);
        source.parameters.set("transparency_amount", 0.45f);
        source.parameters.set("exposure", 1.2f);
        source.textures.set("normal_map", Handle("Textures/NeutralNormal.png"));
        source.textures.set("specular_map", Handle("Textures/NeutralSpecular.png"));
        source.textures.set("shininess_map", Handle("Textures/NeutralShininess.png"));
        source.textures.set("emissive_map", Handle("Textures/NeutralEmissive.png"));

        // Act
        auto copy = PbrMaterialInstance(source);

        // Assert
        EXPECT_EQ(copy.handle.id, source.handle.id);
        EXPECT_TRUE(copy.has_loaded_defaults);
        EXPECT_FLOAT_EQ(copy.get_color().r, 0.25f);
        EXPECT_FLOAT_EQ(copy.get_color().g, 0.5f);
        EXPECT_FLOAT_EQ(copy.get_color().b, 0.75f);
        EXPECT_FLOAT_EQ(copy.get_diffuse_strength(), 0.9f);
        EXPECT_FLOAT_EQ(copy.get_normal_strength(), 0.7f);
        EXPECT_FLOAT_EQ(copy.get_specular_strength(), 0.4f);
        EXPECT_FLOAT_EQ(copy.get_shininess_strength(), 48.0f);
        EXPECT_FLOAT_EQ(copy.get_emissive_color().r, 0.1f);
        EXPECT_FLOAT_EQ(copy.get_emissive_strength(), 0.5f);
        EXPECT_EQ(copy.get_normal_map().name, "Textures/NeutralNormal.png");
        EXPECT_EQ(copy.get_specular_map().name, "Textures/NeutralSpecular.png");
        EXPECT_EQ(copy.get_shininess_map().name, "Textures/NeutralShininess.png");
        EXPECT_EQ(copy.get_emissive_map().name, "Textures/NeutralEmissive.png");
        EXPECT_FLOAT_EQ(copy.get_alpha_cutoff(), 0.3f);
        EXPECT_FLOAT_EQ(copy.get_transparency_amount(), 0.45f);
        EXPECT_FLOAT_EQ(copy.get_exposure(), 1.2f);
    }

    // Validates that converting from a generic MaterialInstance preserves unlit material state.
    TEST(MaterialTests, FlatMaterialInstance_CopyCtor_PreservesLoadedDefaultsState)
    {
        // Arrange
        auto source = MaterialInstance();
        source.handle = unlit_material;
        source.has_loaded_defaults = true;
        source.parameters.set("color", Color(0.6f, 0.4f, 0.2f, 1.0f));
        source.parameters.set("emissive", Color(0.1f, 0.2f, 0.3f, 1.0f));
        source.parameters.set("alpha_cutoff", 0.25f);
        source.parameters.set("transparency_amount", 0.35f);
        source.parameters.set("exposure", 1.4f);
        source.textures.set("diffuse_map", Handle("Textures/FlatAlbedo.png"));

        // Act
        auto copy = FlatMaterialInstance(source);

        // Assert
        EXPECT_EQ(copy.handle.id, source.handle.id);
        EXPECT_TRUE(copy.has_loaded_defaults);
        EXPECT_FLOAT_EQ(copy.get_color().r, 0.6f);
        EXPECT_FLOAT_EQ(copy.get_color().g, 0.4f);
        EXPECT_FLOAT_EQ(copy.get_color().b, 0.2f);
        EXPECT_FLOAT_EQ(copy.get_emissive_color().r, 0.1f);
        EXPECT_FLOAT_EQ(copy.get_emissive_color().g, 0.2f);
        EXPECT_FLOAT_EQ(copy.get_emissive_color().b, 0.3f);
        EXPECT_EQ(copy.get_diffuse_map().name, "Textures/FlatAlbedo.png");
        EXPECT_FLOAT_EQ(copy.get_alpha_cutoff(), 0.25f);
        EXPECT_FLOAT_EQ(copy.get_transparency_amount(), 0.35f);
        EXPECT_FLOAT_EQ(copy.get_exposure(), 1.4f);
    }
}
