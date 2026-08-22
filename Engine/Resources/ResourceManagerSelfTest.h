#pragma once

#include <string>
#include <vector>

class ResourceManagerSelfTest final
{
  public:
	struct Result final
	{
		bool Passed = false;
		std::vector<std::string> Failures;
	};

	// CPU-only, single-threaded diagnostic. It does not create Vulkan objects
	// and does not modify application or renderer state.
	[[nodiscard]] static Result Run();
};
