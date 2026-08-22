#pragma once

#include "../Renderer/MeshRenderer.h"

class Window;

class DebugViewController final
{
  public:
	static void Update(const Window& window, DebugViewMode& currentMode);
	static void PrintHelp();
};
