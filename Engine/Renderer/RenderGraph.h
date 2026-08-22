#pragma once

#include <cstdint>
#include <string>
#include <vector>

class RenderGraph final
{
  public:
    static constexpr std::uint32_t InvalidIndex = UINT32_MAX;

    struct Extent2D
    {
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;
    };

    enum class ImageFormat
    {
        Undefined,
        R8G8B8A8Unorm,
        R16G16B16A16Sfloat,
        D32Sfloat,
        D24UnormS8Uint
    };

    enum class ImageUsage
    {
        Sampled,
        Storage,
        ColorAttachment,
        DepthAttachment,
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
        Extent2D Extent;
        ImageFormat Format = ImageFormat::Undefined;
        std::uint32_t MipLevels = 1;
        std::uint32_t Layers = 1;
        std::uint32_t Samples = 1;
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

    struct PassDescription
    {
        PassId Id;
        std::string Name;
        PassType Type = PassType::Graphics;
        std::vector<ImageUse> Uses;
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
        bool Explicit = false;
    };

    struct ResourceLifetime
    {
        ImageHandle Resource;
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
        std::vector<ValidationIssue> Issues;
    };

    RenderGraph();

    [[nodiscard]] ImageHandle CreateImage(
        std::string name,
        const ImageDescription& description);

    [[nodiscard]] PassId AddPass(
        std::string name,
        PassType type = PassType::Graphics);

    void Read(
        PassId pass,
        ImageHandle resource,
        ImageUsage usage = ImageUsage::Sampled);

    void Write(
        PassId pass,
        ImageHandle resource,
        ImageUsage usage);

    void ReadWrite(
        PassId pass,
        ImageHandle resource,
        ImageUsage usage = ImageUsage::Storage);

    void AddDependency(PassId before, PassId after);

    [[nodiscard]] CompilationResult Compile() const;

    [[nodiscard]] std::string DumpText(const CompilationResult& result) const;
    [[nodiscard]] std::string DumpDot(const CompilationResult& result) const;

    [[nodiscard]] const ImageDescription& GetImageDescription(ImageHandle resource) const;
    [[nodiscard]] const std::string& GetImageName(ImageHandle resource) const;
    [[nodiscard]] const PassDescription& GetPassDescription(PassId pass) const;

  private:
    struct ImageRecord
    {
        ImageHandle Handle;
        std::string Name;
        ImageDescription Description;
    };

    std::uint64_t GraphId = 0;
    std::vector<ImageRecord> Images;
    std::vector<PassDescription> Passes;

    [[nodiscard]] bool Owns(ImageHandle resource) const;
    [[nodiscard]] bool Owns(PassId pass) const;

    void AddUse(
        PassId pass,
        ImageHandle resource,
        AccessType access,
        ImageUsage usage);

    [[nodiscard]] static AccessType CombineAccess(
        AccessType current,
        AccessType requested);

    [[nodiscard]] static bool IncludesRead(AccessType access);
    [[nodiscard]] static bool IncludesWrite(AccessType access);

    [[nodiscard]] static std::string ToString(ImageFormat format);
    [[nodiscard]] static std::string ToString(ImageUsage usage);
    [[nodiscard]] static std::string ToString(AccessType access);
    [[nodiscard]] static std::string ToString(PassType type);
    [[nodiscard]] static std::string EscapeDot(const std::string& value);
};
