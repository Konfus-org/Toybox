#pragma once
#include "tbx/types/window.h"

namespace tbx
{
    /// @brief
    /// Purpose: Provides the runtime API for creating and controlling windows through a service.
    /// @details
    /// Ownership: Implementations own all tracked window state and native resources.
    /// Thread Safety: Not inherently thread-safe; intended for synchronized main-thread use unless
    /// documented otherwise by the concrete implementation.
    class TBX_API IWindowManager
    {
      public:
        virtual ~IWindowManager() noexcept = default;

      public:
        /// @brief
        /// Purpose: Opens a managed window and returns its id.
        /// @details
        /// Ownership: Returns a copied window id owned by the manager.
        /// Thread Safety: Not thread-safe; call from the owning window thread.
        virtual Window open(const WindowCreateInfo& create_info = {}) = 0;

        /// @brief
        /// Purpose: Closes and removes a managed window.
        /// @details
        /// Ownership: The manager releases ownership of the tracked window and its native handle.
        /// Thread Safety: Not thread-safe; call from the owning window thread.
        virtual bool close(const Window& window) = 0;

        /// @brief
        /// Purpose: Reports whether the manager is tracking the given window id.
        /// @details
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Matches the concrete implementation's synchronization guarantees.
        virtual bool has(const Window& window) const = 0;

        /// @brief
        /// Purpose: Reports whether the specified managed window is currently open.
        /// @details
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Matches the concrete implementation's synchronization guarantees.
        virtual bool is_open(const Window& window) const = 0;

        /// @brief
        /// Purpose: Returns the tracked mode for the specified window.
        /// @details
        /// Ownership: Returns a copied enum value.
        /// Thread Safety: Matches the concrete implementation's synchronization guarantees.
        virtual WindowMode get_mode(const Window& window) const = 0;

        /// @brief
        /// Purpose: Updates the tracked and native mode for the specified window.
        /// @details
        /// Ownership: The manager retains ownership of any native resources it mutates.
        /// Thread Safety: Not thread-safe; call from the owning window thread.
        virtual bool set_mode(const Window& window, WindowMode mode) = 0;

        /// @brief
        /// Purpose: Returns the tracked title for the specified window.
        /// @details
        /// Ownership: Returns a copied string owned by the caller.
        /// Thread Safety: Matches the concrete implementation's synchronization guarantees.
        virtual std::string get_title(const Window& window) const = 0;

        /// @brief
        /// Purpose: Updates the tracked and native title for the specified window.
        /// @details
        /// Ownership: The manager retains ownership of the tracked title after copying the input.
        /// Thread Safety: Not thread-safe; call from the owning window thread.
        virtual bool set_title(const Window& window, std::string title) = 0;

        /// @brief
        /// Purpose: Returns the current native handle for the specified window when available.
        /// @details
        /// Ownership: Returns a non-owning backend handle managed by the window manager.
        /// Thread Safety: Matches the concrete implementation's synchronization guarantees.
        virtual NativeWindowHandle get_native_handle(const Window& window) const = 0;

        /// @brief
        /// Purpose: Returns the tracked size for the specified window.
        /// @details
        /// Ownership: Returns a copied size value.
        /// Thread Safety: Matches the concrete implementation's synchronization guarantees.
        virtual Size get_size(const Window& window) const = 0;

        /// @brief
        /// Purpose: Updates the tracked and native size for the specified window.
        /// @details
        /// Ownership: The manager retains ownership of any native resources it mutates.
        /// Thread Safety: Not thread-safe; call from the owning window thread.
        virtual bool set_size(const Window& window, const Size& size) = 0;

        /// @brief
        /// Purpose: Returns all currently open managed windows.
        /// @details
        /// Ownership: Returns a caller-owned copy of the current open window ids.
        /// Thread Safety: Matches the concrete implementation's synchronization guarantees.
        virtual std::vector<Window> get_open_windows() const = 0;

        /// @brief
        /// Purpose: Reports whether a main window is currently designated.
        /// @details
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Matches the concrete implementation's synchronization guarantees.
        virtual bool has_main_window() const = 0;

        /// @brief
        /// Purpose: Returns the designated main window handle.
        /// @details
        /// Ownership: Returns a reference owned by the manager; may be invalid when no main
        /// window is set.
        /// Thread Safety: Matches the concrete implementation's synchronization guarantees.
        virtual const Window& get_main_window() const = 0;

        /// @brief
        /// Purpose: Designates which managed window is treated as the main window.
        /// @details
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Not thread-safe; call from the owning window thread.
        virtual bool set_main_window(const Window& window) = 0;

        /// @brief
        /// Purpose: Processes backend window events and applies pending window operations.
        /// @details
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Not thread-safe; call from the owning window thread.
        virtual void update() = 0;

        /// @brief
        /// Purpose: Closes and releases all managed windows.
        /// @details
        /// Ownership: The manager releases tracked window records and native resources.
        /// Thread Safety: Not thread-safe; call from the owning window thread.
        virtual void shutdown() = 0;
    };
}
