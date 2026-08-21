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
