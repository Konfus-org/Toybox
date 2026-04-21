#pragma once
#include "tbx/common/handle.h"
#include "tbx/math/size.h"
#include "tbx/tbx_api.h"
#include <string>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Carries backend-specific window handle data across systems.
    /// @details
    /// Ownership: Non-owning opaque pointer; platform backend controls lifetime.
    /// Thread Safety: Pointer value is copyable; lifetime access must be externally synchronized.
    using NativeWindowHandle = void*;

    /// @brief
    /// Purpose: Identifies a managed window within the active window manager service.
    /// @details
    /// Ownership: Value type copied by callers; the window manager owns the underlying state.
    /// Thread Safety: Safe to copy and compare across threads.
    using Window = Handle;

    // Enumerates the presentation modes that a window can be configured for.
    // Ownership: Represents value semantics only; no ownership concerns.
    // Thread-safety: Immutable enum used freely across threads.
    enum class WindowMode
    {
        WINDOWED,
        BORDERLESS,
        FULLSCREEN,
        MINIMIZED
    };

    /// @brief
    /// Purpose: Describes the initial state used when a window manager creates a window entry.
    /// @details
    /// Ownership: Owns copied configuration values used during window creation.
    /// Thread Safety: Safe to copy; mutable use must be externally synchronized.
    struct TBX_API WindowCreateInfo
    {
        std::string title = "Toybox";
        Size size = {1280, 720};
        WindowMode mode = WindowMode::WINDOWED;
        bool open_on_creation = true;
    };

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
        /// Purpose: Creates a managed window entry and optionally opens its native window.
        /// @details
        /// Ownership: Returns a copied window id owned by the manager.
        /// Thread Safety: Not thread-safe; call from the owning window thread.
        virtual Window create(const WindowCreateInfo& create_info = {}) = 0;

        /// @brief
        /// Purpose: Removes a managed window entry and releases any native resources it owns.
        /// @details
        /// Ownership: The manager releases ownership of the tracked window and its native handle.
        /// Thread Safety: Not thread-safe; call from the owning window thread.
        virtual bool destroy(const Window& window) = 0;

        /// @brief
        /// Purpose: Reports whether the manager is tracking the given window id.
        /// @details
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Matches the concrete implementation's synchronization guarantees.
        virtual bool has(const Window& window) const = 0;

        /// @brief
        /// Purpose: Opens the native window for an existing managed window entry.
        /// @details
        /// Ownership: The manager retains ownership of the native resources it creates.
        /// Thread Safety: Not thread-safe; call from the owning window thread.
        virtual bool open(const Window& window) = 0;

        /// @brief
        /// Purpose: Closes the native window for an existing managed window entry.
        /// @details
        /// Ownership: The manager releases the native handle but keeps the tracked window entry.
        /// Thread Safety: Not thread-safe; call from the owning window thread.
        virtual bool close(const Window& window) = 0;

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
    };
}
