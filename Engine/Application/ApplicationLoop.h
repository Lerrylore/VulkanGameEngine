#pragma once

#include <functional>

class Window;

class ApplicationLoop final
{
  public:
	ApplicationLoop(Window& window) noexcept;

	ApplicationLoop(const ApplicationLoop&) = delete;
	ApplicationLoop& operator=(const ApplicationLoop&) = delete;
	ApplicationLoop(ApplicationLoop&&) = delete;
	ApplicationLoop& operator=(ApplicationLoop&&) = delete;

	void Run(const std::function<void(float)>& tickCallback) const;

  private:
	Window& WindowReference;
};
