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
        const bool is_material_valid = sky.material.is_valid();

        // Assert
        EXPECT_FALSE(is_material_valid);
    }
}
