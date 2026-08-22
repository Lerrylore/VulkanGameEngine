#pragma once

#include <string>

class Resource
{
  public:
	Resource(std::string resourceId);
	virtual ~Resource() = default;

	Resource(const Resource&) = delete;
	Resource& operator=(const Resource&) = delete;
	Resource(Resource&&) = delete;
	Resource& operator=(Resource&&) = delete;

	[[nodiscard]] const std::string& GetId() const noexcept;
	[[nodiscard]] bool IsLoaded() const noexcept;

	bool Load();
	void Unload();

  protected:
	[[nodiscard]] virtual bool DoLoad() = 0;
	virtual void DoUnload() = 0;

  private:
	std::string ResourceId;
	bool Loaded = false;
};
