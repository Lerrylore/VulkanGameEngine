#pragma once

#include "Resource.h"

#include <vector>

class BinaryResource final : public Resource
{
  public:
	BinaryResource(std::string resourceId, std::string filePath);

	BinaryResource(const BinaryResource&) = delete;
	BinaryResource& operator=(const BinaryResource&) = delete;
	BinaryResource(BinaryResource&&) = delete;
	BinaryResource& operator=(BinaryResource&&) = delete;

	[[nodiscard]] const std::vector<char>& data() const noexcept;
	[[nodiscard]] const std::string& filePath() const noexcept;

  protected:
	[[nodiscard]] bool DoLoad() override;
	void DoUnload() override;

  private:
	std::string FilePath;
	std::vector<char> Data;
};
