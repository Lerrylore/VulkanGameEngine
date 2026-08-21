#pragma once

#include <cstdint>
#include <utility>

struct GLFWwindow;

class Window final
{
  public:
	Window(uint32_t width, uint32_t height, const char* title);
	~Window();

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	Window(Window&&) = delete;
	Window& operator=(Window&&) = delete;

	[[nodiscard]] GLFWwindow* nativeHandle() const noexcept;
	[[nodiscard]] bool shouldClose() const noexcept;
	[[nodiscard]] std::pair<int, int> framebufferSize() const noexcept;
	[[nodiscard]] bool wasFramebufferResized() const noexcept;

	void pollEvents() const noexcept;
	void waitEvents() const noexcept;
	void resetFramebufferResized() noexcept;

  private:
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

	GLFWwindow* handle = nullptr;
	bool framebufferResized = false;
};
