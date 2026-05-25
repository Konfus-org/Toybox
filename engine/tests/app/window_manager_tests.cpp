#include "tbx/interfaces/window_backend.h"
#include "tbx/systems/graphics/messages.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/windowing/window_manager.h"
#include <algorithm>
#include <vector>

namespace tbx::tests::app
{
    namespace internal
    {
        class RecordingWindowBackend final : public IWindowBackend
        {
          public:
            bool create_window(
                const Window& window,
                const WindowCreateInfo&,
                NativeWindowHandle& out_native_handle) override
            {
                open_windows.push_back(window);
                out_native_handle = reinterpret_cast<NativeWindowHandle>(1);
                return true;
            }

            bool destroy_window(const Window& window) override
            {
                const auto window_it = std::ranges::find(open_windows, window);
                if (window_it != open_windows.end())
                    open_windows.erase(window_it);
                return true;
            }

            bool set_window_mode(const Window&, WindowMode) override
            {
                return true;
            }

            bool set_window_title(const Window&, const std::string&) override
            {
                return true;
            }

            bool set_window_size(const Window&, const Size&) override
            {
                return true;
            }

            void pump_events(std::vector<WindowBackendEvent>& out_events) override
            {
                out_events.insert(out_events.end(), pending_events.begin(), pending_events.end());
                pending_events.clear();
            }

            void shutdown() override
            {
                open_windows.clear();
                pending_events.clear();
            }

          public:
            std::vector<Window> open_windows = {};
            std::vector<WindowBackendEvent> pending_events = {};
        };
    }

    TEST(window_manager, close_main_window_clears_main_before_closed_event)
    {
        // Arrange
        auto dispatcher = MessageCoordinator {};
        auto backend = internal::RecordingWindowBackend {};
        auto manager = WindowManager(dispatcher, backend);
        const Window main_window = manager.open(
            WindowCreateInfo {
                .title = "Main",
                .size = {1280, 720},
                .mode = WindowMode::WINDOWED,
            });

        ASSERT_TRUE(main_window.id.is_valid());
        ASSERT_TRUE(manager.has_main_window());

        auto observed_closed_event = false;
        auto observed_has_main_window = true;
        auto observed_is_open = true;
        auto observed_window = Window {};
        dispatcher.register_handler(
            [&](Message& msg)
            {
                const auto closed_event = handle_message<WindowClosedEvent>(msg);
                if (!closed_event.has_value())
                    return;

                observed_closed_event = true;
                observed_has_main_window = manager.has_main_window();
                observed_is_open = manager.is_open(closed_event->get().window);
                observed_window = closed_event->get().window;
            });

        // Act
        const bool closed = manager.close(main_window);

        // Assert
        EXPECT_TRUE(closed);
        EXPECT_TRUE(observed_closed_event);
        EXPECT_EQ(observed_window, main_window);
        EXPECT_FALSE(observed_has_main_window);
        EXPECT_FALSE(observed_is_open);
        EXPECT_FALSE(observed_window.is_valid());
        EXPECT_FALSE(main_window.is_valid());
        EXPECT_FALSE(manager.has_main_window());
        EXPECT_FALSE(manager.has(main_window));
    }
}
