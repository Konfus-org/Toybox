#include "GlfwWindow.h"
#include <GLFW/glfw3native.h>
#include <Core.h>

namespace GlfwWindowing
{
	GlfwWindow::GlfwWindow()
	{
		Toybox::Size size(0, 0);
		_size = size;
		_vSyncEnabled = true;
	}

	GlfwWindow::~GlfwWindow()
	{
		glfwDestroyWindow(_glfwWindow);
	}

	void GlfwWindow::Open(Toybox::WindowMode mode)
	{
		SetMode(mode);
	}

	void GlfwWindow::Update()
	{
		glfwSwapBuffers(_glfwWindow);

		// Needs to be at the end of the update! 
		// Otherwise something like closing will run and anything after could throw errors because the window was destroyed...
		glfwPollEvents(); 
	}

	void GlfwWindow::SetVSyncEnabled(const bool enabled)
	{
		_vSyncEnabled = enabled;
		glfwSwapInterval(_vSyncEnabled);
	}

	bool GlfwWindow::GetVSyncEnabled() const
	{
		return _vSyncEnabled;
	}

	void GlfwWindow::SetSize(Toybox::Size size)
	{
		_size = size;
	}

	Toybox::Size GlfwWindow::GetSize() const
	{
		return _size;
	}

	std::string GlfwWindow::GetTitle() const
	{
		return _title;
	}

	void GlfwWindow::SetTitle(const std::string& title)
	{
		_title = title;
	}

	std::any GlfwWindow::GetNativeWindow() const
	{
		return _glfwWindow;
	}

	Toybox::uint64 GlfwWindow::GetId() const
	{
#ifdef TBX_PLATFORM_WINDOWS
		return (Toybox::uint64)glfwGetWin32Window(_glfwWindow);
#endif
#ifdef TBX_PLATFORM_OSX
		return (Toybox::uint64)glfwGetCocoaWindow(_glfwWindow);
#endif
#ifdef TBX_PLATFORM_LINUX
		return (Toybox::uint64)glfwGetX11Window(_glfwWindow);
#endif
	}

	void GlfwWindow::SetEventCallback(const EventCallbackFn& callback)
	{
		_eventCallback = callback;
		SetupCallbacks();
	}

	void GlfwWindow::SetMode(Toybox::WindowMode mode)
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
			case Toybox::WindowMode::Windowed:
			{
				// Set window hints for a windowed window
				glfwWindowHint(GLFW_REFRESH_RATE, videoMode->refreshRate); // Match refresh rate

				// Create window
				_glfwWindow = glfwCreateWindow(_size.Width, _size.Height, _title.c_str(), nullptr, nullptr);
				break;
			}
			// Borderless mode (monitor = nullptr, decorated = false)
			case Toybox::WindowMode::Borderless:
			{
				// Set window hints for a borderless window
				glfwWindowHint(GLFW_DECORATED, false); // Disable window decorations
				glfwWindowHint(GLFW_REFRESH_RATE, videoMode->refreshRate); // Match refresh rate

				// Create window
				_glfwWindow = glfwCreateWindow(_size.Width, _size.Height, _title.c_str(), nullptr, nullptr);
				break;
			}
			// Fullscreen mode (monitor != nullptr)
			case Toybox::WindowMode::Fullscreen:
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
			case Toybox::WindowMode::FullscreenBorderless:
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

		SetVSyncEnabled(_vSyncEnabled);
		SetupContext();
		SetupCallbacks();
	}

	void GlfwWindow::SetupCallbacks()
	{
		glfwSetErrorCallback([](int error, const char* description)
		{
			TBX_CRITICAL("Glfw error: ({0}): {1}", error, description);
		});

		glfwSetFramebufferSizeCallback(_glfwWindow, [](GLFWwindow* window, int width, int height)
		{
			// Tell glfw to redraw while resizing
			glfwSwapBuffers(window);
		});

		glfwSetWindowSizeCallback(_glfwWindow, [](GLFWwindow* window, int width, int height)
		{
			GlfwWindow& toyboxWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);
			toyboxWindow.SetSize(Toybox::Size(width, height));
			Toybox::WindowResizeEvent event(toyboxWindow.GetId(), width, height);
			toyboxWindow._eventCallback(event);
		});

		glfwSetWindowCloseCallback(_glfwWindow, [](GLFWwindow* window)
		{
			const GlfwWindow& toyboxWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);
			Toybox::WindowCloseEvent event(toyboxWindow.GetId());
			toyboxWindow._eventCallback(event);
		});

		glfwSetKeyCallback(_glfwWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			const GlfwWindow& toyboxWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);

			switch (action)
			{
				case GLFW_PRESS:
				{
					Toybox::KeyPressedEvent event(key);
					toyboxWindow._eventCallback(event);
					break;
				}
				case GLFW_RELEASE:
				{
					Toybox::KeyReleasedEvent event(key);
					toyboxWindow._eventCallback(event);
					break;
				}
				case GLFW_REPEAT:
				{
					Toybox::KeyRepeatedEvent event(key, 1);
					toyboxWindow._eventCallback(event);
					break;
				}
				default:
				{
					TBX_ASSERT(false, "Unhandled key press action!");
					break;
				}
			}
		});

		glfwSetCharCallback(_glfwWindow, [](GLFWwindow* window, unsigned int keycode)
		{
			const GlfwWindow& toyboxWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);

			Toybox::KeyPressedEvent event(keycode);
			toyboxWindow._eventCallback(event);
		});

		glfwSetMouseButtonCallback(_glfwWindow, [](GLFWwindow* window, int button, int action, int mods)
		{
			const GlfwWindow& toyboxWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);

			switch (action)
			{
				case GLFW_PRESS:
				{
					Toybox::MouseButtonPressedEvent event(button);
					toyboxWindow._eventCallback(event);
					break;
				}
				case GLFW_RELEASE:
				{
					Toybox::MouseButtonReleasedEvent event(button);
					toyboxWindow._eventCallback(event);
					break;
				}
				default:
				{
					TBX_ASSERT(false, "Unhandled mouse button press action!");
					break;
				}
			}
		});

		glfwSetScrollCallback(_glfwWindow, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			const GlfwWindow& toyboxWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);

			Toybox::MouseScrolledEvent event((float)xOffset, (float)yOffset);
			toyboxWindow._eventCallback(event);
		});

		glfwSetCursorPosCallback(_glfwWindow, [](GLFWwindow* window, double xPos, double yPos)
		{
			const GlfwWindow& toyboxWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);

			Toybox::MouseMovedEvent event((float)xPos, (float)yPos);
			toyboxWindow._eventCallback(event);
		});
	}

	void GlfwWindow::SetupContext()
	{
		glfwMakeContextCurrent(_glfwWindow);
		glfwSetWindowUserPointer(_glfwWindow, this);
	}
}