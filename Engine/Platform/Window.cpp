#include "Window.h"

#include <stdexcept>

#include <GLFW/glfw3.h>

Window::Window(uint32_t width, uint32_t height, const char* title)
{
	if (glfwInit() != GLFW_TRUE)
	{
		throw std::runtime_error("failed to initialize GLFW!");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	handle = glfwCreateWindow(
		static_cast<int>(width),
		static_cast<int>(height),
		title,
		nullptr,
		nullptr);
	if (handle == nullptr)
	{
		glfwTerminate();
		throw std::runtime_error("failed to create GLFW window!");
	}

	glfwSetWindowUserPointer(handle, this);
	glfwSetFramebufferSizeCallback(handle, framebufferResizeCallback);
}

Window::~Window()
{
	glfwDestroyWindow(handle);
	glfwTerminate();
}

GLFWwindow* Window::nativeHandle() const noexcept
{
	return handle;
}

bool Window::shouldClose() const noexcept
{
	return glfwWindowShouldClose(handle) != 0;
}

std::pair<int, int> Window::framebufferSize() const noexcept
{
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(handle, &width, &height);
	return {width, height};
}

bool Window::wasFramebufferResized() const noexcept
{
	return framebufferResized;
}

bool Window::IsKeyDown(WindowKey key) const noexcept
{
	int glfwKey = GLFW_KEY_UNKNOWN;
	switch (key)
	{
	case WindowKey::Number0:
		glfwKey = GLFW_KEY_0;
		break;
	case WindowKey::Number1:
		glfwKey = GLFW_KEY_1;
		break;
	case WindowKey::Number2:
		glfwKey = GLFW_KEY_2;
		break;
	case WindowKey::Number3:
		glfwKey = GLFW_KEY_3;
		break;
	case WindowKey::Number4:
		glfwKey = GLFW_KEY_4;
		break;
	case WindowKey::Number5:
		glfwKey = GLFW_KEY_5;
		break;
	case WindowKey::Number6:
		glfwKey = GLFW_KEY_6;
		break;
	case WindowKey::Number7:
		glfwKey = GLFW_KEY_7;
		break;
	case WindowKey::Number8:
		glfwKey = GLFW_KEY_8;
		break;
	case WindowKey::Number9:
		glfwKey = GLFW_KEY_9;
		break;
	case WindowKey::F1:
		glfwKey = GLFW_KEY_F1;
		break;
	case WindowKey::F2:
		glfwKey = GLFW_KEY_F2;
		break;
	case WindowKey::F3:
		glfwKey = GLFW_KEY_F3;
		break;
	case WindowKey::F4:
		glfwKey = GLFW_KEY_F4;
		break;
	case WindowKey::F5:
		glfwKey = GLFW_KEY_F5;
		break;
	case WindowKey::F6:
		glfwKey = GLFW_KEY_F6;
		break;
	case WindowKey::F7:
		glfwKey = GLFW_KEY_F7;
		break;
	}

	return glfwGetKey(handle, glfwKey) == GLFW_PRESS;
}

void Window::pollEvents() const noexcept
{
	glfwPollEvents();
}

void Window::waitEvents() const noexcept
{
	glfwWaitEvents();
}

void Window::resetFramebufferResized() noexcept
{
	framebufferResized = false;
}

void Window::framebufferResizeCallback(GLFWwindow* window, int, int)
{
	auto* owner = static_cast<Window*>(glfwGetWindowUserPointer(window));
	owner->framebufferResized = true;
}
