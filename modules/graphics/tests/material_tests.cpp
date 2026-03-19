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
        source.parameters.set("metallic", 0.4f);
        source.parameters.set("roughness", 0.6f);
        source.parameters.set("emissive", Color(0.1f, 0.2f, 0.3f, 1.0f));
        source.parameters.set("occlusion", 0.8f);
        source.parameters.set("alpha_cutoff", 0.3f);
        source.parameters.set("exposure", 1.2f);
        source.textures.set("orm", Handle("Textures/NeutralOrm.png"));

        // Act
        auto copy = StandardMaterialInstance(source);

        // Assert
        EXPECT_EQ(copy.handle.id, source.handle.id);
        EXPECT_TRUE(copy.has_loaded_defaults);
        EXPECT_FLOAT_EQ(copy.get_color().r, 0.25f);
        EXPECT_FLOAT_EQ(copy.get_color().g, 0.5f);
        EXPECT_FLOAT_EQ(copy.get_color().b, 0.75f);
        EXPECT_FLOAT_EQ(copy.get_metallic(), 0.4f);
        EXPECT_FLOAT_EQ(copy.get_roughness(), 0.6f);
        EXPECT_FLOAT_EQ(copy.get_emissive().r, 0.1f);
        EXPECT_FLOAT_EQ(copy.get_occlusion(), 0.8f);
        EXPECT_EQ(copy.get_orm().name, "Textures/NeutralOrm.png");
        EXPECT_FLOAT_EQ(copy.get_alpha_cutoff(), 0.3f);
        EXPECT_FLOAT_EQ(copy.get_exposure(), 1.2f);
    }
}
