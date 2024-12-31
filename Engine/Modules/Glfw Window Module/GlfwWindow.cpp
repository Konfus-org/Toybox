#include "GLFWWindow.h"
#include <GLFW/glfw3native.h>
#include <Core.h>

namespace GLFWWindowing
{
	GLFWWindow::GLFWWindow()
	{
		Toybox::Size size(0, 0);
		_size = size;
		_vSyncEnabled = true;
	}

	GLFWWindow::~GLFWWindow()
	{
		glfwDestroyWindow(_glfwWindow);
	}

	void GLFWWindow::SetRenderer(const std::shared_ptr<Toybox::IRenderer>& renderer)
	{
		_renderer = renderer;
		_renderer->SetViewport({ 0, 0 }, _size);
	}

	void GLFWWindow::Open(const Toybox::WindowMode& mode)
	{
		SetMode(mode);
	}

	void GLFWWindow::Update()
	{
		// Draw basic color to window for now
		// TODO: use a queue to pass things to renderer
		_renderer->BeginFrame();

		_renderer->Draw(Toybox::Color(20, 20, 30, 255));

		//// Testing / Drawing triangle
		const auto& mesh = Toybox::Mesh::Triangle();
		_renderer->Draw(mesh, Toybox::Vector3(), Toybox::Quaternion(), Toybox::Scale());
		//// Testing / Drawing triangle

		_renderer->EndFrame();

		// Needs to be at the end of the update! 
		// Otherwise something like closing will run and anything after could throw errors because the window was destroyed...
		glfwPollEvents();
	}

	void GLFWWindow::SetVSyncEnabled(const bool& enabled)
	{
		_vSyncEnabled = enabled;
		_renderer->SetVSyncEnabled(enabled);
	}

	bool GLFWWindow::GetVSyncEnabled() const
	{
		return _vSyncEnabled;
	}

	void GLFWWindow::SetSize(const Toybox::Size& size)
	{
		_size = size;
		if (_glfwWindow != nullptr)
		{
			glfwSetWindowSize(_glfwWindow, _size.Width, _size.Height);
			OnSizeChanged();
		}
	}

	Toybox::Size GLFWWindow::GetSize() const
	{
		return _size;
	}

	std::string GLFWWindow::GetTitle() const
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

	Toybox::uint64 GLFWWindow::GetId() const
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

	void GLFWWindow::SetEventCallback(const Toybox::EventCallbackFn& callback)
	{
		_eventCallback = callback;
		SetupCallbacks();
	}

	void GLFWWindow::SetMode(const Toybox::WindowMode& mode)
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
				Toybox::KeyPressedEvent event(key);
				_eventCallback(event);
				break;
			}
			case GLFW_RELEASE:
			{
				Toybox::KeyReleasedEvent event(key);
				_eventCallback(event);
				break;
			}
			case GLFW_REPEAT:
			{
				Toybox::KeyRepeatedEvent event(key, 1);
				_eventCallback(event);
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
				Toybox::MouseButtonPressedEvent event(button);
				_eventCallback(event);
				break;
			}
			case GLFW_RELEASE:
			{
				Toybox::MouseButtonReleasedEvent event(button);
				_eventCallback(event);
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
		Toybox::MouseScrolledEvent event((float)offsetX, (float)offsetY);
		_eventCallback(event);
	}

	void GLFWWindow::OnMouseMoved(double posX, double posY) const
	{
		Toybox::MouseMovedEvent event((float)posX, (float)posY);
		_eventCallback(event);
	}

	void GLFWWindow::OnWindowClosed() const
	{
		Toybox::WindowCloseEvent event(GetId());
		_eventCallback(event);
	}

	void GLFWWindow::OnSizeChanged()
	{
		if (_renderer != nullptr)
		{
			_renderer->BeginFrame();
			_renderer->SetViewport({ 0, 0 }, _size);
			_renderer->EndFrame();
		}
		Toybox::WindowResizeEvent event(GetId(), _size.Width, _size.Height);
		_eventCallback(event);
	}
}