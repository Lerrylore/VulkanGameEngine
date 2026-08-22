#pragma once

#include "BufferAllocation.h"

class MeshResource final
{
  public:
	MeshResource(
		VulkanContext& vulkan,
		vk::DeviceSize bufferSize,
		vk::DeviceSize vertexOffset,
		vk::DeviceSize indexOffset,
		vk::IndexType indexType,
		uint32_t indexCount);

	MeshResource(const MeshResource&) = delete;
	MeshResource& operator=(const MeshResource&) = delete;
	MeshResource(MeshResource&&) noexcept = default;
	MeshResource& operator=(MeshResource&&) = delete;

	[[nodiscard]] vk::raii::Buffer& buffer() noexcept;
	[[nodiscard]] const vk::raii::Buffer& buffer() const noexcept;
	[[nodiscard]] vk::DeviceSize vertexOffset() const noexcept;
	[[nodiscard]] vk::DeviceSize indexOffset() const noexcept;
	[[nodiscard]] vk::IndexType indexType() const noexcept;
	[[nodiscard]] uint32_t indexCount() const noexcept;

  private:
	BufferAllocation geometryBuffer;
	vk::DeviceSize vertexBufferOffset = 0;
	vk::DeviceSize indexBufferOffset = 0;
	vk::IndexType drawIndexType = vk::IndexType::eUint32;
	uint32_t drawIndexCount = 0;
};
