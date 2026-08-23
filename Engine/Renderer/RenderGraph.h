#pragma once

#ifndef VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#endif

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <string>
#include <vector>

class RenderGraph final
{
  public:
    static constexpr std::uint32_t InvalidIndex = UINT32_MAX;

    enum class ImageUsage
    {
        Sampled,
        Storage,
        ColorAttachment,
        DepthAttachment,
        TransferSource,
        TransferDestination
    };

    enum class BufferUsage
    {
        Storage,
        Vertex,
        Index,
        Uniform,
        TransferSource,
        TransferDestination
    };

    enum class AccessType
    {
        Read,
        Write,
        ReadWrite
    };

    enum class PassType
    {
        Graphics,
        Compute,
        Transfer
    };

    struct ImageDescription
    {
        vk::Format Format = vk::Format::eUndefined;
        vk::Extent2D Extent{};
        vk::ImageUsageFlags Usage{};
        vk::ImageLayout InitialLayout = vk::ImageLayout::eUndefined;
        vk::ImageLayout FinalLayout = vk::ImageLayout::eUndefined;
        std::uint32_t MipLevels = 1;
        std::uint32_t Layers = 1;
        vk::SampleCountFlagBits Samples = vk::SampleCountFlagBits::e1;
        bool Imported = false;
        bool Transient = true;
    };

    struct BufferDescription
    {
        std::uint64_t Size = 0;
        bool Imported = false;
        bool Transient = true;
    };

    struct ImageHandle
    {
        std::uint32_t Index = InvalidIndex;
        std::uint64_t GraphId = 0;

        [[nodiscard]] bool IsValid() const;
        friend bool operator==(const ImageHandle&, const ImageHandle&) = default;
    };

    struct BufferHandle
    {
        std::uint32_t Index = InvalidIndex;
        std::uint64_t GraphId = 0;

        [[nodiscard]] bool IsValid() const;
        friend bool operator==(const BufferHandle&, const BufferHandle&) = default;
    };

    struct PassId
    {
        std::uint32_t Index = InvalidIndex;
        std::uint64_t GraphId = 0;

        [[nodiscard]] bool IsValid() const;
        friend bool operator==(const PassId&, const PassId&) = default;
    };

    struct ImageUse
    {
        ImageHandle Resource;
        AccessType Access = AccessType::Read;
        ImageUsage Usage = ImageUsage::Sampled;
    };

    struct BufferUse
    {
        BufferHandle Resource;
        AccessType Access = AccessType::Read;
        BufferUsage Usage = BufferUsage::Storage;
    };

    struct PassDescription
    {
        PassId Id;
        std::string Name;
        PassType Type = PassType::Graphics;
        std::vector<ImageUse> Uses;
        std::vector<BufferUse> BufferUses;
        std::vector<PassId> ExplicitDependencies;
    };

    struct ValidationIssue
    {
        enum class Severity
        {
            Warning,
            Error
        };

        Severity Level = Severity::Error;
        std::string Message;
    };

    struct Dependency
    {
        PassId From;
        PassId To;
        std::vector<ImageHandle> Resources;
        std::vector<BufferHandle> BufferResources;
        bool Explicit = false;
    };

    struct ResourceLifetime
    {
        ImageHandle Resource;
        std::uint32_t FirstPassOrder = InvalidIndex;
        std::uint32_t LastPassOrder = InvalidIndex;
        std::uint32_t UseCount = 0;
    };

    struct BufferLifetime
    {
        BufferHandle Resource;
        std::uint32_t FirstPassOrder = InvalidIndex;
        std::uint32_t LastPassOrder = InvalidIndex;
        std::uint32_t UseCount = 0;
    };

    struct CompilationResult
    {
        bool Succeeded = false;
        std::vector<PassId> ExecutionOrder;
        std::vector<Dependency> Dependencies;
        std::vector<ResourceLifetime> ResourceLifetimes;
        std::vector<BufferLifetime> BufferLifetimes;
        std::vector<ValidationIssue> Issues;
    };

    RenderGraph();

    // Mirrors the tutorial's graph.AddResource(...): describes an image and
    // its Vulkan contract. Imported images are bound later by the frame code.
    [[nodiscard]] ImageHandle AddResource(
        std::string name,
        const ImageDescription& description);

    [[nodiscard]] BufferHandle AddBuffer(
        std::string name,
        const BufferDescription& description);

    [[nodiscard]] PassId AddPass(
        std::string name,
        PassType type = PassType::Graphics);

    void Read(
        PassId pass,
        ImageHandle resource,
        ImageUsage usage = ImageUsage::Sampled);

    void Read(
        PassId pass,
        BufferHandle resource,
        BufferUsage usage = BufferUsage::Storage);

    void Write(
        PassId pass,
        ImageHandle resource,
        ImageUsage usage);

    void Write(
        PassId pass,
        BufferHandle resource,
        BufferUsage usage);

    void ReadWrite(
        PassId pass,
        ImageHandle resource,
        ImageUsage usage = ImageUsage::Storage);

    void ReadWrite(
        PassId pass,
        BufferHandle resource,
        BufferUsage usage = BufferUsage::Storage);

    void AddDependency(PassId before, PassId after);

    [[nodiscard]] CompilationResult Compile() const;

    [[nodiscard]] std::string DumpText(const CompilationResult& result) const;
    [[nodiscard]] std::string DumpDot(const CompilationResult& result) const;

    [[nodiscard]] const ImageDescription& GetImageDescription(ImageHandle resource) const;
    [[nodiscard]] const std::string& GetImageName(ImageHandle resource) const;
    [[nodiscard]] const BufferDescription& GetBufferDescription(BufferHandle resource) const;
    [[nodiscard]] const std::string& GetBufferName(BufferHandle resource) const;
    [[nodiscard]] const PassDescription& GetPassDescription(PassId pass) const;

  private:
    struct ImageRecord
    {
        ImageHandle Handle;
        std::string Name;
        ImageDescription Description;
    };

    struct BufferRecord
    {
        BufferHandle Handle;
        std::string Name;
        BufferDescription Description;
    };

    std::uint64_t GraphId = 0;
    std::vector<ImageRecord> Images;
    std::vector<BufferRecord> Buffers;
    std::vector<PassDescription> Passes;

    [[nodiscard]] bool Owns(ImageHandle resource) const;
    [[nodiscard]] bool Owns(BufferHandle resource) const;
    [[nodiscard]] bool Owns(PassId pass) const;

    void AddUse(
        PassId pass,
        ImageHandle resource,
        AccessType access,
        ImageUsage usage);

    void AddUse(
        PassId pass,
        BufferHandle resource,
        AccessType access,
        BufferUsage usage);

    [[nodiscard]] static AccessType CombineAccess(
        AccessType current,
        AccessType requested);

    [[nodiscard]] static bool IncludesRead(AccessType access);
    [[nodiscard]] static bool IncludesWrite(AccessType access);

    [[nodiscard]] static std::string ToString(vk::Format format);
    [[nodiscard]] static std::string ToString(ImageUsage usage);
    [[nodiscard]] static std::string ToString(BufferUsage usage);
    [[nodiscard]] static std::string ToString(AccessType access);
    [[nodiscard]] static std::string ToString(PassType type);
    [[nodiscard]] static std::string EscapeDot(const std::string& value);
};
