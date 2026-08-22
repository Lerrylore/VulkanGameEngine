#include "ResourceManagerSelfTest.h"

#include "ResourceManager.h"

#include <memory>
#include <utility>

namespace
{
class SelfTestResource final : public Resource
{
  public:
	explicit SelfTestResource(std::string resourceId)
		: Resource(std::move(resourceId))
	{
	}

	[[nodiscard]] int LoadCount() const noexcept
	{
		return Loads;
	}

	[[nodiscard]] int UnloadCount() const noexcept
	{
		return Unloads;
	}

  protected:
	bool DoLoad() override
	{
		++Loads;
		return true;
	}

	void DoUnload() override
	{
		++Unloads;
	}

  private:
	int Loads = 0;
	int Unloads = 0;
};
}

ResourceManagerSelfTest::Result ResourceManagerSelfTest::Run()
{
	Result result;
	auto expect = [&result](bool condition, std::string message)
	{
		if (!condition)
		{
			result.Failures.push_back(std::move(message));
		}
	};

	ResourceManager resources;
	int factoryCalls = 0;
	std::shared_ptr<SelfTestResource> observedResource;

	auto factory = [&factoryCalls, &observedResource]()
	{
		++factoryCalls;
		observedResource = std::make_shared<SelfTestResource>("self_test");
		return observedResource;
	};

	ResourceHandle<SelfTestResource> firstHandle =
		resources.LoadWithFactory<SelfTestResource>("self_test", factory);
	expect(firstHandle.IsValid(), "factory load should return a valid handle");
	expect(factoryCalls == 1, "factory should be called once for the first load");
	expect(resources.HasResource<SelfTestResource>("self_test"), "loaded resource should be cached");

	ResourceHandle<SelfTestResource> secondHandle =
		resources.LoadWithFactory<SelfTestResource>("self_test", factory);
	expect(secondHandle.IsValid(), "cached load should return a valid handle");
	expect(factoryCalls == 1, "cached load should not call the factory again");
	expect(firstHandle.Get() == secondHandle.Get(), "cached handles should point to the same resource");

	firstHandle.Reset();
	expect(resources.HasResource<SelfTestResource>("self_test"), "one remaining handle should keep the resource cached");
	expect(observedResource->IsLoaded(), "one remaining handle should keep the resource loaded");

	secondHandle.Reset();
	expect(!resources.HasResource<SelfTestResource>("self_test"), "last handle should remove the resource from the cache");
	expect(!observedResource->IsLoaded(), "last handle should unload the resource");
	expect(observedResource->UnloadCount() == 1, "last handle should unload exactly once");

	std::shared_ptr<SelfTestResource> reloadResource;
	ResourceHandle<SelfTestResource> reloadHandle = resources.LoadWithFactory<SelfTestResource>(
		"reload_test",
		[&reloadResource]()
		{
			reloadResource = std::make_shared<SelfTestResource>("reload_test");
			return reloadResource;
		});
	expect(reloadHandle.IsValid(), "reload resource should load successfully");
	expect(resources.Reload<SelfTestResource>("reload_test"), "cached resource should reload successfully");
	expect(reloadResource->LoadCount() == 2, "reload should call DoLoad a second time");
	expect(reloadResource->UnloadCount() == 1, "reload should unload before loading again");

	reloadHandle.Reset();
	expect(reloadResource->UnloadCount() == 2, "releasing the reloaded resource should unload it");

	result.Passed = result.Failures.empty();
	return result;
}
