#include "debug_window_title_monitor.h"

namespace tbx::tests::app
{
    TEST(PerformanceMonitorTitleTests, BuildDebugWindowTitle_ShowsPlaceholdersDuringWarmup)
    {
        // Arrange
        auto monitor = performance_monitor::DebugWindowTitleMonitor();
        constexpr auto FRAME_DT = DeltaTime {.seconds = 1.0 / 60.0, .milliseconds = 1000.0 / 60.0};
        std::optional<std::string> title = std::nullopt;

        // Act
        for (int frame = 0; frame < 16; ++frame)
        {
            monitor.record_frame(FRAME_DT);
            title = monitor.consume_title("Toybox App", GraphicsApi::OPEN_GL);
            if (title.has_value())
                break;
        }

        // Assert
        ASSERT_TRUE(title.has_value());
        EXPECT_EQ(*title, "Toybox App [opengl, FPS: ---, Frame: --- ms]");
    }

    TEST(PerformanceMonitorTitleTests, BuildDebugWindowTitle_ShowsFpsAndFrameTimeAfterWarmup)
    {
        // Arrange
        auto monitor = performance_monitor::DebugWindowTitleMonitor();
        constexpr auto FRAME_DT = DeltaTime {.seconds = 1.0 / 60.0, .milliseconds = 1000.0 / 60.0};
        std::optional<std::string> title = std::nullopt;

        // Act
        for (int frame = 0; frame < 90; ++frame)
        {
            monitor.record_frame(FRAME_DT);
            if (const auto next_title = monitor.consume_title("Toybox App", GraphicsApi::OPEN_GL))
                title = next_title;
        }

        // Assert
        ASSERT_TRUE(title.has_value());
        EXPECT_EQ(*title, "Toybox App [opengl, FPS: 60, Frame: 16.67 ms]");
    }
}
