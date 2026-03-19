#include "PCH.h"
#include "tbx/common/handle.h"
#include "tbx/graphics/material.h"

namespace tbx::tests::graphics
{
    // Validates that converting from a generic MaterialInstance preserves default hydration state.
    TEST(MaterialTests, StandardMaterialInstance_CopyCtor_PreservesLoadedDefaultsState)
    {
        // Arrange
        auto source = MaterialInstance();
        source.handle = Handle(Uuid(0x00000002U));
        source.has_loaded_defaults = true;
        source.parameters.set("color", Color(0.25f, 0.5f, 0.75f, 1.0f));
        source.parameters.set("diffuse_map_strength", 0.9f);
        source.parameters.set("normal_map_strength", 0.7f);
        source.parameters.set("specular", 0.4f);
        source.parameters.set("specular_map_strength", 0.6f);
        source.parameters.set("shininess", 48.0f);
        source.parameters.set("shininess_map_strength", 0.8f);
        source.parameters.set("emissive", Color(0.1f, 0.2f, 0.3f, 1.0f));
        source.parameters.set("emissive_map_strength", 0.5f);
        source.parameters.set("occlusion", 0.8f);
        source.parameters.set("occlusion_map_strength", 0.4f);
        source.parameters.set("alpha_cutoff", 0.3f);
        source.parameters.set("transparency_amount", 0.45f);
        source.parameters.set("exposure", 1.2f);
        source.textures.set("normal_map", Handle("Textures/NeutralNormal.png"));
        source.textures.set("specular_map", Handle("Textures/NeutralSpecular.png"));
        source.textures.set("shininess_map", Handle("Textures/NeutralShininess.png"));
        source.textures.set("emissive_map", Handle("Textures/NeutralEmissive.png"));
        source.textures.set("occlusion_map", Handle("Textures/NeutralOcclusion.png"));

        // Act
        auto copy = StandardMaterialInstance(source);

        // Assert
        EXPECT_EQ(copy.handle.id, source.handle.id);
        EXPECT_TRUE(copy.has_loaded_defaults);
        EXPECT_FLOAT_EQ(copy.get_color().r, 0.25f);
        EXPECT_FLOAT_EQ(copy.get_color().g, 0.5f);
        EXPECT_FLOAT_EQ(copy.get_color().b, 0.75f);
        EXPECT_FLOAT_EQ(copy.get_diffuse_map_strength(), 0.9f);
        EXPECT_FLOAT_EQ(copy.get_normal_map_strength(), 0.7f);
        EXPECT_FLOAT_EQ(copy.get_specular(), 0.4f);
        EXPECT_FLOAT_EQ(copy.get_specular_map_strength(), 0.6f);
        EXPECT_FLOAT_EQ(copy.get_shininess(), 48.0f);
        EXPECT_FLOAT_EQ(copy.get_shininess_map_strength(), 0.8f);
        EXPECT_FLOAT_EQ(copy.get_emissive().r, 0.1f);
        EXPECT_FLOAT_EQ(copy.get_emissive_map_strength(), 0.5f);
        EXPECT_FLOAT_EQ(copy.get_occlusion(), 0.8f);
        EXPECT_FLOAT_EQ(copy.get_occlusion_map_strength(), 0.4f);
        EXPECT_EQ(copy.get_normal_map().name, "Textures/NeutralNormal.png");
        EXPECT_EQ(copy.get_specular_map().name, "Textures/NeutralSpecular.png");
        EXPECT_EQ(copy.get_shininess_map().name, "Textures/NeutralShininess.png");
        EXPECT_EQ(copy.get_emissive_map().name, "Textures/NeutralEmissive.png");
        EXPECT_EQ(copy.get_occlusion_map().name, "Textures/NeutralOcclusion.png");
        EXPECT_FLOAT_EQ(copy.get_alpha_cutoff(), 0.3f);
        EXPECT_FLOAT_EQ(copy.get_transparency_amount(), 0.45f);
        EXPECT_FLOAT_EQ(copy.get_exposure(), 1.2f);
    }
}
