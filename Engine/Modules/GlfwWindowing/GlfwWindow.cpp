#include "GlfwWindow.h"
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <glad/glad.h>
#include <Toybox.h>

namespace GlfwWindowing
{
	static bool s_glfwInitialized = false;
	GLFWwindow* _glfwWindow;

	GlfwWindow::GlfwWindow()
	{
		_vSyncEnabled = true;
		_size = nullptr;
	}

	GlfwWindow::~GlfwWindow()
	{
		glfwDestroyWindow(_glfwWindow);
		delete _size;
	}

	void GlfwWindow::InitGlad()
	{
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		TBX_ASSERT(status, "Failed to initialize glad!");
	}

	void GlfwWindow::InitGlfwIfNotAlreadyInitialized()
	{
		if (!s_glfwInitialized)
		{
			const bool success = glfwInit();
			TBX_ASSERT(success, "Failed to initialize glfw!");
			s_glfwInitialized = true;
		}
	}

	void GlfwWindow::Open(Toybox::WindowMode mode)
	{
		InitGlfwIfNotAlreadyInitialized();
		SetMode(mode);
		InitGlad();
	}

	void GlfwWindow::Update()
	{
		glfwPollEvents();
		glfwSwapBuffers(_glfwWindow);
	}

	void GlfwWindow::SetVSyncEnabled(const bool enabled)
	{
		_vSyncEnabled = enabled;
		glfwSwapInterval(_vSyncEnabled);
	}

	const bool GlfwWindow::GetVSyncEnabled() const
	{
		return _vSyncEnabled;
	}

	void GlfwWindow::SetSize(Toybox::Size* size)
	{
		delete _size;
		_size = size;
	}

	const Toybox::Size* GlfwWindow::GetSize() const
	{
		return _size;
	}

	const std::string GlfwWindow::GetTitle() const
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

	const Toybox::uint64 GlfwWindow::GetId() const
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
	}

	void GlfwWindow::SetMode(Toybox::WindowMode mode)
	{
		if (_glfwWindow != nullptr)
		{
			glfwDestroyWindow(_glfwWindow);
		}

		switch (mode)
		{
			//Windowed mode (monitor = nullptr)
			case Toybox::WindowMode::Windowed:
			{
				_glfwWindow = glfwCreateWindow((int)_size->Width, (int)_size->Height, _title.c_str(), nullptr, nullptr);
				break;
			}
			// Fullscreen mode (monitor != nullptr)
			case Toybox::WindowMode::Fullscreen:
			{
				_glfwWindow = glfwCreateWindow((int)_size->Width, (int)_size->Height, _title.c_str(), glfwGetPrimaryMonitor(), nullptr);
				break;
			}
			// Borderless (monitor = nullptr, decorated = false)
			case Toybox::WindowMode::Borderless:
			{
				glfwWindowHint(GLFW_DECORATED, false);
				_glfwWindow = glfwCreateWindow((int)_size->Width, (int)_size->Height, _title.c_str(), nullptr, nullptr);
				break;
			}
			// Fullscreen borderless (monitor != nullptr, video mode = monitor mode)
			case Toybox::WindowMode::FullscreenBorderless:
			{
				_glfwWindow = glfwCreateWindow((int)_size->Width, (int)_size->Height, _title.c_str(), glfwGetPrimaryMonitor(), nullptr);
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
			glClear(GL_COLOR_BUFFER_BIT);
			glfwSwapBuffers(window);
		});

		glfwSetWindowSizeCallback(_glfwWindow, [](GLFWwindow* window, int width, int height)
		{
			GlfwWindow& toyboxWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);
			toyboxWindow.SetSize(new Toybox::Size(width, height));

			Toybox::WindowResizeEvent event(width, height);
			toyboxWindow._eventCallback(event);
		});

		glfwSetWindowCloseCallback(_glfwWindow, [](GLFWwindow* window)
		{
			GlfwWindow& toyboxWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);
			Toybox::WindowCloseEvent event;
			toyboxWindow._eventCallback(event);
		});

		glfwSetKeyCallback(_glfwWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			GlfwWindow& toyboxWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);

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
			}
		});

		glfwSetCharCallback(_glfwWindow, [](GLFWwindow* window, unsigned int keycode)
		{
			GlfwWindow& toyboxWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);

			Toybox::KeyPressedEvent event(keycode);
			toyboxWindow._eventCallback(event);
		});

		glfwSetMouseButtonCallback(_glfwWindow, [](GLFWwindow* window, int button, int action, int mods)
		{
			GlfwWindow& toyboxWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);

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
			}
		});

		glfwSetScrollCallback(_glfwWindow, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			GlfwWindow& toyboxWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);

			Toybox::MouseScrolledEvent event((float)xOffset, (float)yOffset);
			toyboxWindow._eventCallback(event);
		});

		glfwSetCursorPosCallback(_glfwWindow, [](GLFWwindow* window, double xPos, double yPos)
		{
			GlfwWindow& toyboxWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);

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