#include "GLFWWindow.h"
#include <GLFW/glfw3native.h>
#include <Tbx/Systems/Events/EventCoordinator.h>
#include <Tbx/Systems/Windowing/WindowEvents.h>
#include <Tbx/Systems/Input/InputEvents.h>
#include <Tbx/Systems/Debug/Debugging.h>

namespace GLFWWindowing
{
	GLFWWindow::GLFWWindow()
	{
		Tbx::Size size(0, 0);
		_size = size;
	}

	GLFWWindow::~GLFWWindow()
	{
		glfwDestroyWindow(_glfwWindow);
	}

	void GLFWWindow::Open(const Tbx::WindowMode& mode)
	{
		SetMode(mode);
		SetupCallbacks();
	}

	void GLFWWindow::Close()
	{
        glfwDestroyWindow(_glfwWindow);
	}

	void GLFWWindow::DrawFrame()
	{
		glfwPollEvents();
	}

	void GLFWWindow::Focus()
	{
        glfwFocusWindow(_glfwWindow);
		OnWindowFocusChanged(true);
	}

	void GLFWWindow::SetMode(const Tbx::WindowMode& mode)
	{
		if (_glfwWindow != nullptr)
		{
			glfwDestroyWindow(_glfwWindow);
		}

		GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* videoMode = glfwGetVideoMode(primaryMonitor);

		switch (mode)
		{
			// Windowed mode (monitor = nullptr)
		case Tbx::WindowMode::Windowed:
		{
			// Set window hints for a windowed window
			glfwWindowHint(GLFW_REFRESH_RATE, videoMode->refreshRate); // Match refresh rate

			// Create window
			_glfwWindow = glfwCreateWindow(_size.Width, _size.Height, _title.c_str(), nullptr, nullptr);
			break;
		}
		// Borderless mode (monitor = nullptr, decorated = false)
		case Tbx::WindowMode::Borderless:
		{
			// Set window hints for a borderless window
			glfwWindowHint(GLFW_DECORATED, false); // Disable window decorations
			glfwWindowHint(GLFW_REFRESH_RATE, videoMode->refreshRate); // Match refresh rate

			// Create window
			_glfwWindow = glfwCreateWindow(_size.Width, _size.Height, _title.c_str(), nullptr, nullptr);
			break;
		}
		// Fullscreen mode (monitor != nullptr)
		case Tbx::WindowMode::Fullscreen:
		{
			// Set window hints for a fullscreen window
			glfwWindowHint(GLFW_RESIZABLE, false); // Make it non-resizable
			glfwWindowHint(GLFW_REFRESH_RATE, videoMode->refreshRate); // Match refresh rate

			// Create window
			_glfwWindow = glfwCreateWindow(videoMode->width, videoMode->height, _title.c_str(), nullptr, nullptr);

			// Position the window at (0, 0) to cover the whole screen
			glfwSetWindowPos(_glfwWindow, 0, 0);
			break;
		}
		// Fullscreen borderless mode (monitor != nullptr, video mode = monitor mode)
		case Tbx::WindowMode::FullscreenBorderless:
		{
			// Set window hints for a borderless window
			glfwWindowHint(GLFW_DECORATED, false); // Disable window decorations
			glfwWindowHint(GLFW_RESIZABLE, false); // Make it non-resizable
			glfwWindowHint(GLFW_REFRESH_RATE, videoMode->refreshRate); // Match refresh rate

			// Create window
			_glfwWindow = glfwCreateWindow(videoMode->width, videoMode->height, _title.c_str(), nullptr, nullptr);

			// Position the window at (0, 0) to cover the whole screen
			glfwSetWindowPos(_glfwWindow, 0, 0);
			break;
		}
		}

		glfwSetWindowUserPointer(_glfwWindow, this);
		SetupCallbacks();
	}

	void GLFWWindow::SetSize(const Tbx::Size& size)
	{
		_size = size;
		if (_glfwWindow != nullptr)
		{
			glfwSetWindowSize(_glfwWindow, _size.Width, _size.Height);
			OnSizeChanged();
		}
	}

	const Tbx::Size& GLFWWindow::GetSize() const
	{
		return _size;
	}

	const std::string& GLFWWindow::GetTitle() const
	{
		return _title;
	}

	void GLFWWindow::SetTitle(const std::string& title)
	{
		_title = title;
	}

	Tbx::NativeHandle GLFWWindow::GetNativeHandle() const
	{
#ifdef TBX_PLATFORM_WINDOWS
		return reinterpret_cast<Tbx::NativeHandle>(glfwGetWin32Window(_glfwWindow));
#endif
#ifdef TBX_PLATFORM_OSX
		return  reinterpret_cast<Tbx::NativeHandle>(glfwGetCocoaWindow(_glfwWindow));
#endif
#ifdef TBX_PLATFORM_LINUX
		return  reinterpret_cast<Tbx::NativeHandle>(glfwGetX11Window(_glfwWindow));
#endif
	}

    Tbx::NativeWindow GLFWWindow::GetNativeWindow() const
	{
		return _glfwWindow;
	}

	void GLFWWindow::SetupCallbacks()
	{
		glfwSetErrorCallback([](int error, const char* description)
		{
			TBX_CRITICAL("Glfw error: ({0}): {1}", error, description);
		});

		glfwSetWindowSizeCallback(_glfwWindow, [](GLFWwindow* window, int width, int height)
		{
			GLFWWindow& toyboxWindow = *(GLFWWindow*)glfwGetWindowUserPointer(window);
			toyboxWindow._size = { width, height };
			toyboxWindow.OnSizeChanged();
		});

		glfwSetWindowFocusCallback(_glfwWindow, [](GLFWwindow* window, int focused)
        {
            const GLFWWindow& toyboxWindow = *(GLFWWindow*)glfwGetWindowUserPointer(window);
            toyboxWindow.OnWindowFocusChanged(focused);
        });

		glfwSetWindowCloseCallback(_glfwWindow, [](GLFWwindow* window)
		{
			const GLFWWindow& toyboxWindow = *(GLFWWindow*)glfwGetWindowUserPointer(window);
            toyboxWindow.OnWindowClosed();
		});

		glfwSetKeyCallback(_glfwWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			const GLFWWindow& toyboxWindow = *(GLFWWindow*)glfwGetWindowUserPointer(window);
			toyboxWindow.OnKeyPressed(key, scancode, action, mods);
		});

		glfwSetCharCallback(_glfwWindow, [](GLFWwindow* window, unsigned int keycode)
		{
			const GLFWWindow& toyboxWindow = *(GLFWWindow*)glfwGetWindowUserPointer(window);
			toyboxWindow.OnKeyPressed(keycode, 0, GLFW_PRESS, 0);
		});

		glfwSetMouseButtonCallback(_glfwWindow, [](GLFWwindow* window, int button, int action, int mods)
		{
			const GLFWWindow& toyboxWindow = *(GLFWWindow*)glfwGetWindowUserPointer(window);
			toyboxWindow.OnMouseButtonPressed(button, action, mods);
		});

		glfwSetScrollCallback(_glfwWindow, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			const GLFWWindow& toyboxWindow = *(GLFWWindow*)glfwGetWindowUserPointer(window);
			toyboxWindow.OnMouseScrolled(xOffset, yOffset);
		});

		glfwSetCursorPosCallback(_glfwWindow, [](GLFWwindow* window, double xPos, double yPos)
		{
			const GLFWWindow& toyboxWindow = *(GLFWWindow*)glfwGetWindowUserPointer(window);
            toyboxWindow.OnMouseMoved(xPos, yPos);
		});
	}

	void GLFWWindow::OnKeyPressed(int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) const
	{
		switch (action)
		{
			case GLFW_PRESS:
			{
				Tbx::KeyPressedEvent event(key);
				Tbx::EventCoordinator::Send(event);
				break;
			}
			case GLFW_RELEASE:
			{
				Tbx::KeyReleasedEvent event(key);
				Tbx::EventCoordinator::Send(event);
				break;
			}
			case GLFW_REPEAT:
			{
				Tbx::KeyRepeatedEvent event(key, 1);
				Tbx::EventCoordinator::Send(event);
				break;
			}
			default:
			{
				TBX_ASSERT(false, "Unhandled key press action!");
				break;
			}
		}
	}

	void GLFWWindow::OnMouseButtonPressed(int button, int action, [[maybe_unused]] int mods) const
	{
		switch (action)
		{
			case GLFW_PRESS:
			{
				Tbx::MouseButtonPressedEvent event(button);
				Tbx::EventCoordinator::Send(event);
				break;
			}
			case GLFW_RELEASE:
			{
				Tbx::MouseButtonReleasedEvent event(button);
				Tbx::EventCoordinator::Send(event);
				break;
			}
			default:
			{
				TBX_ASSERT(false, "Unhandled mouse button press action!");
				break;
			}
		}
	}

	void GLFWWindow::OnMouseScrolled(double offsetX, double offsetY) const
	{
		Tbx::MouseScrolledEvent event((float)offsetX, (float)offsetY);
		Tbx::EventCoordinator::Send(event);
	}

	void GLFWWindow::OnMouseMoved(double posX, double posY) const
	{
		Tbx::MouseMovedEvent event((float)posX, (float)posY);
		Tbx::EventCoordinator::Send(event);
	}

	void GLFWWindow::OnWindowClosed() const
	{
		Tbx::WindowClosedEvent event(Id);
		Tbx::EventCoordinator::Send(event);
	}

	void GLFWWindow::OnWindowFocusChanged(bool isFocused) const
	{
        Tbx::WindowFocusChangedEvent event(Id, isFocused);
        Tbx::EventCoordinator::Send(event);
	}

	void GLFWWindow::OnSizeChanged() const
	{
		Tbx::WindowResizedEvent event(Id, _size.Width, _size.Height);
		Tbx::EventCoordinator::Send(event);
	}
}