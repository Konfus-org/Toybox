#include "GLFWWindow.h"
#include <GLFW/glfw3native.h>

namespace GLFWWindowing
{
	GLFWWindow::GLFWWindow()
	{
		Tbx::Size size(0, 0);
		_size = size;
		_camera = std::make_shared<Tbx::Camera>();
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

	void GLFWWindow::Update()
	{
		glfwPollEvents();
	}

	std::weak_ptr<Tbx::Camera> GLFWWindow::GetCamera() const
	{
        return _camera;
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

	std::any GLFWWindow::GetNativeWindow() const
	{
		return _glfwWindow;
	}

	Tbx::UID GLFWWindow::GetId() const
	{
#ifdef TBX_PLATFORM_WINDOWS
		return (Tbx::uint64)glfwGetWin32Window(_glfwWindow);
#endif
#ifdef TBX_PLATFORM_OSX
		return (Tbx::uint64)glfwGetCocoaWindow(_glfwWindow);
#endif
#ifdef TBX_PLATFORM_LINUX
		return (Tbx::uint64)glfwGetX11Window(_glfwWindow);
#endif
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
				Tbx::Events::Send(event);
				break;
			}
			case GLFW_RELEASE:
			{
				Tbx::KeyReleasedEvent event(key);
				Tbx::Events::Send(event);
				break;
			}
			case GLFW_REPEAT:
			{
				Tbx::KeyRepeatedEvent event(key, 1);
				Tbx::Events::Send(event);
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
				Tbx::Events::Send(event);
				break;
			}
			case GLFW_RELEASE:
			{
				Tbx::MouseButtonReleasedEvent event(button);
				Tbx::Events::Send(event);
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
		Tbx::Events::Send(event);
	}

	void GLFWWindow::OnMouseMoved(double posX, double posY) const
	{
		Tbx::MouseMovedEvent event((float)posX, (float)posY);
		Tbx::Events::Send(event);
	}

	void GLFWWindow::OnWindowClosed() const
	{
		Tbx::WindowClosedEvent event(GetId());
		Tbx::Events::Send(event);
	}

	void GLFWWindow::OnSizeChanged() const
	{
		Tbx::WindowResizedEvent event(GetId(), _size.Width, _size.Height);
		Tbx::Events::Send(event);
	}
}