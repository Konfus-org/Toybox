#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/api.h"
#include "tbx/systems/plugin_api/loaded_plugin.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/systems/time/span.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/bounds.h"

namespace tbx::tests::utils
{
    TEST(format_tests, FormatsToyboxTypesWithStdFormat)
    {
        // Arrange
        auto world = World {};
        auto entity = world.create_persistent_entity("Player");
        auto plugin = LoadedPlugin {};
        plugin.meta.name = "Renderer";
        plugin.meta.version = "1.0.0";

        // Act
        const std::string uuid_text = std::format("{}", Uuid(0x2AU));
        const std::string handle_text = std::format("{}", Handle("Texture", Uuid(0x11U)));
        const std::string bounds_text = std::format("{}", Bounds(-1.0F, 1.0F, 2.0F, -2.0F));
        const std::string time_span_text =
            std::format("{}", TimeSpan {.value = 5U, .unit = TimeUnit::SECONDS});
        const std::string delta_time_text =
            std::format("{}", DeltaTime {.seconds = 0.5, .milliseconds = 500.0});
        const std::string entity_text = std::format("{}", entity);
        const std::string graphics_api_text = std::format("{}", GraphicsApi::OPEN_GL);
        const std::string plugin_text = std::format("{}", plugin);

        // Assert
        EXPECT_EQ(uuid_text, "2a");
        EXPECT_EQ(handle_text, "[Name: Texture, Id: 11]");
        EXPECT_EQ(bounds_text, "[Left: -1, Right: 1, Top: 2, Bottom: -2]");
        EXPECT_EQ(time_span_text, "5 s");
        EXPECT_EQ(delta_time_text, "0.5s");
        EXPECT_NE(entity_text.find("Entity{id="), std::string::npos);
        EXPECT_NE(entity_text.find("name='Player'"), std::string::npos);
        EXPECT_NE(entity_text.find("parent=0"), std::string::npos);
        EXPECT_EQ(graphics_api_text, "opengl");
        EXPECT_EQ(plugin_text, "Name=Renderer, Version=1.0.0");

        TBX_TRACE_INFO(
            "Formatter smoke: {} {} {} {} {}",
            Uuid(0x2AU),
            Handle("Texture", Uuid(0x11U)),
            Bounds(),
            DeltaTime {},
            GraphicsApi::OPEN_GL);
    }

    TEST(format_tests, HandleCopiesShareInvalidation)
    {
        // Arrange
        const auto source = Handle("SharedHandle", Uuid(0x44U));
        const auto copy = source;

        // Act
        source.invalidate();

        // Assert
        EXPECT_FALSE(source.is_valid());
        EXPECT_FALSE(copy.is_valid());
        EXPECT_TRUE(source.id.is_valid());
        EXPECT_TRUE(copy.id.is_valid());
    }
}
