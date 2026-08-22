#include "MeshResource.h"

#include <utility>

MeshResource::MeshResource(
	std::string resourceId,
	VulkanContext& vulkan,
	vk::DeviceSize bufferSize,
	vk::DeviceSize vertexOffset,
	vk::DeviceSize indexOffset,
	vk::IndexType indexType,
	uint32_t indexCount)
	: Resource(std::move(resourceId)),
	  geometryBuffer{
		std::in_place,
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
	return geometryBuffer->buffer();
}

const vk::raii::Buffer& MeshResource::buffer() const noexcept
{
	return geometryBuffer->buffer();
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

bool MeshResource::DoLoad()
{
	return geometryBuffer.has_value();
}

void MeshResource::DoUnload()
{
	geometryBuffer.reset();
}
