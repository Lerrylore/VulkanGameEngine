#include "ApplicationLoop.h"

#include "../Platform/Window.h"

#include <chrono>

ApplicationLoop::ApplicationLoop(Window& window) noexcept
	: WindowReference(window)
{
}

void ApplicationLoop::Run(const std::function<void(float)>& tickCallback) const
{
	auto lastUpdate = std::chrono::steady_clock::now();
	while (!WindowReference.shouldClose())
	{
		WindowReference.pollEvents();
		const auto now = std::chrono::steady_clock::now();
		const float deltaTime = std::chrono::duration<float>(now - lastUpdate).count();
		lastUpdate = now;
		tickCallback(deltaTime);
	}
}
