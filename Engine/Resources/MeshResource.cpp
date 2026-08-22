#include "MeshResource.h"

MeshResource::MeshResource(
	VulkanContext& vulkan,
	vk::DeviceSize bufferSize,
	vk::DeviceSize vertexOffset,
	vk::DeviceSize indexOffset,
	vk::IndexType indexType,
	uint32_t indexCount)
	: geometryBuffer{
		vulkan,
		bufferSize,
		vk::BufferUsageFlagBits::eVertexBuffer |
			vk::BufferUsageFlagBits::eIndexBuffer |
			vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal},
	  vertexBufferOffset{vertexOffset},
	  indexBufferOffset{indexOffset},
	  drawIndexType{indexType},
	  drawIndexCount{indexCount}
{
}

vk::raii::Buffer& MeshResource::buffer() noexcept
{
	return geometryBuffer.buffer();
}

const vk::raii::Buffer& MeshResource::buffer() const noexcept
{
	return geometryBuffer.buffer();
}

vk::DeviceSize MeshResource::vertexOffset() const noexcept
{
	return vertexBufferOffset;
}

vk::DeviceSize MeshResource::indexOffset() const noexcept
{
	return indexBufferOffset;
}

vk::IndexType MeshResource::indexType() const noexcept
{
	return drawIndexType;
}

uint32_t MeshResource::indexCount() const noexcept
{
	return drawIndexCount;
}
