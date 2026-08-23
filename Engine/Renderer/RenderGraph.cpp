#include "RenderGraph.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <optional>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace
{
    std::atomic<std::uint64_t> NextGraphId = 1;

    struct DependencyKey
    {
        std::uint32_t From = RenderGraph::InvalidIndex;
        std::uint32_t To = RenderGraph::InvalidIndex;

        friend bool operator<(const DependencyKey& left, const DependencyKey& right)
        {
            return std::tie(left.From, left.To) < std::tie(right.From, right.To);
        }
    };

    void AppendIssue(
        std::vector<RenderGraph::ValidationIssue>& issues,
        RenderGraph::ValidationIssue::Severity severity,
        std::string message)
    {
        issues.push_back({severity, std::move(message)});
    }

    std::string PassLabel(const RenderGraph& graph, RenderGraph::PassId pass)
    {
        return graph.GetPassDescription(pass).Name;
    }

    vk::ImageUsageFlags RequiredImageUsage(RenderGraph::ImageUsage usage)
    {
        switch (usage)
        {
            case RenderGraph::ImageUsage::Sampled:
                return vk::ImageUsageFlagBits::eSampled;
            case RenderGraph::ImageUsage::Storage:
                return vk::ImageUsageFlagBits::eStorage;
            case RenderGraph::ImageUsage::ColorAttachment:
                return vk::ImageUsageFlagBits::eColorAttachment;
            case RenderGraph::ImageUsage::DepthAttachment:
                return vk::ImageUsageFlagBits::eDepthStencilAttachment;
            case RenderGraph::ImageUsage::TransferSource:
                return vk::ImageUsageFlagBits::eTransferSrc;
            case RenderGraph::ImageUsage::TransferDestination:
                return vk::ImageUsageFlagBits::eTransferDst;
        }
        return {};
    }
}

bool RenderGraph::ImageHandle::IsValid() const
{
    return Index != InvalidIndex && GraphId != 0;
}

bool RenderGraph::BufferHandle::IsValid() const
{
    return Index != InvalidIndex && GraphId != 0;
}

bool RenderGraph::PassId::IsValid() const
{
    return Index != InvalidIndex && GraphId != 0;
}

RenderGraph::RenderGraph()
    : GraphId(NextGraphId.fetch_add(1))
{
}

RenderGraph::ImageHandle RenderGraph::AddResource(
    std::string name,
    const ImageDescription& description)
{
    if (name.empty())
    {
        throw std::invalid_argument("RenderGraph image name cannot be empty");
    }

    if (description.Extent.width == 0 || description.Extent.height == 0)
    {
        throw std::invalid_argument("RenderGraph image extent must be non-zero");
    }

    if (description.Format == vk::Format::eUndefined)
    {
        throw std::invalid_argument("RenderGraph image format cannot be Undefined");
    }

    if (description.Usage == vk::ImageUsageFlags{})
    {
        throw std::invalid_argument("RenderGraph image usage cannot be empty");
    }

    if (description.FinalLayout == vk::ImageLayout::eUndefined)
    {
        throw std::invalid_argument("RenderGraph image final layout cannot be Undefined");
    }

    if (description.MipLevels == 0 || description.Layers == 0 ||
        description.Samples == vk::SampleCountFlagBits{})
    {
        throw std::invalid_argument("RenderGraph image dimensions must be non-zero");
    }

    const auto duplicate = std::find_if(
        Images.begin(),
        Images.end(),
        [&name](const ImageRecord& image) { return image.Name == name; });

    if (duplicate != Images.end())
    {
            throw std::invalid_argument("RenderGraph image name is duplicated: " + name);
    }

    const auto duplicateBuffer = std::find_if(
        Buffers.begin(),
        Buffers.end(),
        [&name](const BufferRecord& buffer) { return buffer.Name == name; });
    if (duplicateBuffer != Buffers.end())
    {
        throw std::invalid_argument("RenderGraph resource name is duplicated: " + name);
    }

    const ImageHandle handle{static_cast<std::uint32_t>(Images.size()), GraphId};
    Images.push_back({handle, std::move(name), description});
    return handle;
}

RenderGraph::BufferHandle RenderGraph::AddBuffer(
    std::string name,
    const BufferDescription& description)
{
    if (name.empty())
    {
        throw std::invalid_argument("RenderGraph buffer name cannot be empty");
    }
    if (description.Size == 0)
    {
        throw std::invalid_argument("RenderGraph buffer size must be non-zero");
    }

    const auto duplicateImage = std::find_if(
        Images.begin(),
        Images.end(),
        [&name](const ImageRecord& image) { return image.Name == name; });
    const auto duplicateBuffer = std::find_if(
        Buffers.begin(),
        Buffers.end(),
        [&name](const BufferRecord& buffer) { return buffer.Name == name; });
    if (duplicateImage != Images.end() || duplicateBuffer != Buffers.end())
    {
        throw std::invalid_argument("RenderGraph resource name is duplicated: " + name);
    }

    const BufferHandle handle{static_cast<std::uint32_t>(Buffers.size()), GraphId};
    Buffers.push_back({handle, std::move(name), description});
    return handle;
}

RenderGraph::PassId RenderGraph::AddPass(std::string name, PassType type)
{
    if (name.empty())
    {
        throw std::invalid_argument("RenderGraph pass name cannot be empty");
    }

    const PassId id{static_cast<std::uint32_t>(Passes.size()), GraphId};
    Passes.push_back({id, std::move(name), type, {}, {}, {}});
    return id;
}

void RenderGraph::Read(PassId pass, ImageHandle resource, ImageUsage usage)
{
    AddUse(pass, resource, AccessType::Read, usage);
}

void RenderGraph::Read(PassId pass, BufferHandle resource, BufferUsage usage)
{
    AddUse(pass, resource, AccessType::Read, usage);
}

void RenderGraph::Write(PassId pass, ImageHandle resource, ImageUsage usage)
{
    AddUse(pass, resource, AccessType::Write, usage);
}

void RenderGraph::Write(PassId pass, BufferHandle resource, BufferUsage usage)
{
    AddUse(pass, resource, AccessType::Write, usage);
}

void RenderGraph::ReadWrite(PassId pass, ImageHandle resource, ImageUsage usage)
{
    AddUse(pass, resource, AccessType::ReadWrite, usage);
}

void RenderGraph::ReadWrite(PassId pass, BufferHandle resource, BufferUsage usage)
{
    AddUse(pass, resource, AccessType::ReadWrite, usage);
}

void RenderGraph::AddDependency(PassId before, PassId after)
{
    if (!Owns(before) || !Owns(after))
    {
        throw std::invalid_argument("RenderGraph dependency references an unknown pass");
    }

    auto& dependencies = Passes[after.Index].ExplicitDependencies;
    if (std::find(dependencies.begin(), dependencies.end(), before) == dependencies.end())
    {
        dependencies.push_back(before);
    }
}

RenderGraph::CompilationResult RenderGraph::Compile() const
{
    CompilationResult result;
    std::map<DependencyKey, Dependency> dependencies;

    const auto addDependency = [&dependencies](
                                   PassId from,
                                   PassId to,
                                   ImageHandle resource,
                                   bool explicitDependency) {
        if (from == to)
        {
            return;
        }

        const DependencyKey key{from.Index, to.Index};
        auto iterator = dependencies.try_emplace(
            key,
            Dependency{from, to, {}, {}, explicitDependency});

        iterator.first->second.Explicit = iterator.first->second.Explicit || explicitDependency;
        if (resource.IsValid() &&
            std::find(
                iterator.first->second.Resources.begin(),
                iterator.first->second.Resources.end(),
                resource) == iterator.first->second.Resources.end())
        {
            iterator.first->second.Resources.push_back(resource);
        }
    };

    const auto addBufferDependency = [&dependencies, &addDependency](
                                         PassId from,
                                         PassId to,
                                         BufferHandle resource) {
        addDependency(from, to, {}, false);
        const DependencyKey key{from.Index, to.Index};
        auto& dependency = dependencies.at(key);
        if (resource.IsValid() &&
            std::find(
                dependency.BufferResources.begin(),
                dependency.BufferResources.end(),
                resource) == dependency.BufferResources.end())
        {
            dependency.BufferResources.push_back(resource);
        }
    };

    struct ResourceState
    {
        std::optional<PassId> LastWriter;
        std::vector<PassId> Readers;
    };

    std::vector<ResourceState> states(Images.size());
    std::vector<ResourceState> bufferStates(Buffers.size());

    for (const auto& pass : Passes)
    {
        if (pass.Uses.empty() && pass.BufferUses.empty())
        {
            AppendIssue(
                result.Issues,
                ValidationIssue::Severity::Warning,
                "Pass '" + pass.Name + "' has no declared resource uses");
        }

        for (const auto& use : pass.Uses)
        {
            if (!Owns(use.Resource))
            {
                AppendIssue(
                    result.Issues,
                    ValidationIssue::Severity::Error,
                    "Pass '" + pass.Name + "' references an image owned by another graph");
                continue;
            }

            const auto& image = Images[use.Resource.Index];
            const auto requiredUsage = RequiredImageUsage(use.Usage);
            if ((image.Description.Usage & requiredUsage) != requiredUsage)
            {
                AppendIssue(
                    result.Issues,
                    ValidationIssue::Severity::Error,
                    "Pass '" + pass.Name + "' uses image '" + image.Name + "' as " +
                        ToString(use.Usage) + " but its vk::ImageUsageFlags do not allow it");
                continue;
            }

            auto& state = states[use.Resource.Index];
            const bool hasWriter = state.LastWriter.has_value();

            if (IncludesRead(use.Access))
            {
                if (!hasWriter && !image.Description.Imported)
                {
                    AppendIssue(
                        result.Issues,
                        ValidationIssue::Severity::Error,
                        "Transient image '" + image.Name + "' is read before its first write in pass '" +
                            pass.Name + "'");
                }

                if (state.LastWriter.has_value())
                {
                    addDependency(*state.LastWriter, pass.Id, use.Resource, false);
                }

                if (std::find(state.Readers.begin(), state.Readers.end(), pass.Id) == state.Readers.end())
                {
                    state.Readers.push_back(pass.Id);
                }
            }

            if (IncludesWrite(use.Access))
            {
                if (state.LastWriter.has_value())
                {
                    addDependency(*state.LastWriter, pass.Id, use.Resource, false);
                }

                for (const auto reader : state.Readers)
                {
                    addDependency(reader, pass.Id, use.Resource, false);
                }

                state.Readers.clear();
                state.LastWriter = pass.Id;
            }
        }

        for (const auto& use : pass.BufferUses)
        {
            if (!Owns(use.Resource))
            {
                AppendIssue(
                    result.Issues,
                    ValidationIssue::Severity::Error,
                    "Pass '" + pass.Name + "' references a buffer owned by another graph");
                continue;
            }

            auto& state = bufferStates[use.Resource.Index];
            const auto& buffer = Buffers[use.Resource.Index];
            const bool hasWriter = state.LastWriter.has_value();

            if (IncludesRead(use.Access))
            {
                if (!hasWriter && !buffer.Description.Imported)
                {
                    AppendIssue(
                        result.Issues,
                        ValidationIssue::Severity::Error,
                        "Transient buffer '" + buffer.Name + "' is read before its first write in pass '" +
                            pass.Name + "'");
                }

                if (state.LastWriter.has_value())
                {
                    addBufferDependency(*state.LastWriter, pass.Id, use.Resource);
                }

                if (std::find(state.Readers.begin(), state.Readers.end(), pass.Id) == state.Readers.end())
                {
                    state.Readers.push_back(pass.Id);
                }
            }

            if (IncludesWrite(use.Access))
            {
                if (state.LastWriter.has_value())
                {
                    addBufferDependency(*state.LastWriter, pass.Id, use.Resource);
                }

                for (const auto reader : state.Readers)
                {
                    addBufferDependency(reader, pass.Id, use.Resource);
                }

                state.Readers.clear();
                state.LastWriter = pass.Id;
            }
        }

        for (const auto dependency : pass.ExplicitDependencies)
        {
            if (dependency == pass.Id)
            {
                AppendIssue(
                    result.Issues,
                    ValidationIssue::Severity::Error,
                    "Pass '" + pass.Name + "' cannot depend on itself");
            }
            else if (Owns(dependency))
            {
                addDependency(dependency, pass.Id, {}, true);
            }
        }
    }

    result.Dependencies.reserve(dependencies.size());
    for (auto& entry : dependencies)
    {
        result.Dependencies.push_back(std::move(entry.second));
    }

    std::vector<std::vector<std::uint32_t>> adjacency(Passes.size());
    std::vector<std::uint32_t> indegree(Passes.size(), 0);

    for (const auto& dependency : result.Dependencies)
    {
        adjacency[dependency.From.Index].push_back(dependency.To.Index);
        ++indegree[dependency.To.Index];
    }

    std::priority_queue<
        std::uint32_t,
        std::vector<std::uint32_t>,
        std::greater<>> ready;

    for (std::uint32_t passIndex = 0; passIndex < indegree.size(); ++passIndex)
    {
        if (indegree[passIndex] == 0)
        {
            ready.push(passIndex);
        }
    }

    while (!ready.empty())
    {
        const auto passIndex = ready.top();
        ready.pop();
        result.ExecutionOrder.push_back(Passes[passIndex].Id);

        for (const auto nextPass : adjacency[passIndex])
        {
            if (--indegree[nextPass] == 0)
            {
                ready.push(nextPass);
            }
        }
    }

    if (result.ExecutionOrder.size() != Passes.size())
    {
        AppendIssue(
            result.Issues,
            ValidationIssue::Severity::Error,
            "RenderGraph dependency cycle detected; topological sort did not visit every pass");
    }

    std::vector<std::uint32_t> passOrder(Passes.size(), InvalidIndex);
    for (std::uint32_t order = 0; order < result.ExecutionOrder.size(); ++order)
    {
        passOrder[result.ExecutionOrder[order].Index] = order;
    }

    result.ResourceLifetimes.reserve(Images.size());
    for (const auto& image : Images)
    {
        ResourceLifetime lifetime;
        lifetime.Resource = image.Handle;

        for (const auto& pass : Passes)
        {
            const auto order = passOrder[pass.Id.Index];
            if (order == InvalidIndex)
            {
                continue;
            }

            for (const auto& use : pass.Uses)
            {
                if (use.Resource != image.Handle)
                {
                    continue;
                }

                if (lifetime.UseCount == 0)
                {
                    lifetime.FirstPassOrder = order;
                    lifetime.LastPassOrder = order;
                }
                else
                {
                    lifetime.FirstPassOrder = std::min(lifetime.FirstPassOrder, order);
                    lifetime.LastPassOrder = std::max(lifetime.LastPassOrder, order);
                }
                ++lifetime.UseCount;
            }
        }

        if (lifetime.UseCount == 0)
        {
            AppendIssue(
                result.Issues,
                ValidationIssue::Severity::Warning,
                "Image '" + image.Name + "' is declared but never used");
        }

        result.ResourceLifetimes.push_back(lifetime);
    }

    result.BufferLifetimes.reserve(Buffers.size());
    for (const auto& buffer : Buffers)
    {
        BufferLifetime lifetime;
        lifetime.Resource = buffer.Handle;

        for (const auto& pass : Passes)
        {
            const auto order = passOrder[pass.Id.Index];
            if (order == InvalidIndex)
            {
                continue;
            }

            for (const auto& use : pass.BufferUses)
            {
                if (use.Resource != buffer.Handle)
                {
                    continue;
                }

                if (lifetime.UseCount == 0)
                {
                    lifetime.FirstPassOrder = order;
                    lifetime.LastPassOrder = order;
                }
                else
                {
                    lifetime.FirstPassOrder = std::min(lifetime.FirstPassOrder, order);
                    lifetime.LastPassOrder = std::max(lifetime.LastPassOrder, order);
                }
                ++lifetime.UseCount;
            }
        }

        if (lifetime.UseCount == 0)
        {
            AppendIssue(
                result.Issues,
                ValidationIssue::Severity::Warning,
                "Buffer '" + buffer.Name + "' is declared but never used");
        }

        result.BufferLifetimes.push_back(lifetime);
    }

    result.Succeeded = std::none_of(
        result.Issues.begin(),
        result.Issues.end(),
        [](const ValidationIssue& issue) {
            return issue.Level == ValidationIssue::Severity::Error;
        });
    return result;
}

std::string RenderGraph::DumpText(const CompilationResult& result) const
{
    std::ostringstream output;
    output << "RenderGraph: " << (result.Succeeded ? "valid" : "invalid") << '\n';

    output << "Resources:\n";
    for (const auto& image : Images)
    {
        const auto lifetime = std::find_if(
            result.ResourceLifetimes.begin(),
            result.ResourceLifetimes.end(),
            [&image](const ResourceLifetime& value) { return value.Resource == image.Handle; });

        output << "  [" << image.Handle.Index << "] " << image.Name << " "
               << image.Description.Extent.width << 'x' << image.Description.Extent.height << ' '
               << ToString(image.Description.Format) << " usage=" << vk::to_string(image.Description.Usage)
               << " initial=" << vk::to_string(image.Description.InitialLayout)
               << " final=" << vk::to_string(image.Description.FinalLayout)
               << " samples=" << static_cast<std::uint32_t>(image.Description.Samples)
               << (image.Description.Imported ? " imported" : " transient");

        if (lifetime != result.ResourceLifetimes.end() && lifetime->UseCount != 0)
        {
            output << " lifetime=" << lifetime->FirstPassOrder << ".." << lifetime->LastPassOrder
                   << " uses=" << lifetime->UseCount;
        }
        output << '\n';
    }

    output << "Buffers:\n";
    for (const auto& buffer : Buffers)
    {
        const auto lifetime = std::find_if(
            result.BufferLifetimes.begin(),
            result.BufferLifetimes.end(),
            [&buffer](const BufferLifetime& value) { return value.Resource == buffer.Handle; });

        output << "  [" << buffer.Handle.Index << "] " << buffer.Name << " size="
               << buffer.Description.Size << " bytes"
               << (buffer.Description.Imported ? " imported" : " transient");
        if (lifetime != result.BufferLifetimes.end() && lifetime->UseCount != 0)
        {
            output << " lifetime=" << lifetime->FirstPassOrder << ".." << lifetime->LastPassOrder
                   << " uses=" << lifetime->UseCount;
        }
        output << '\n';
    }

    output << "Execution order:\n";
    for (std::uint32_t order = 0; order < result.ExecutionOrder.size(); ++order)
    {
        const auto pass = result.ExecutionOrder[order];
        const auto& description = GetPassDescription(pass);
        output << "  [" << order << "] " << description.Name << " (" << ToString(description.Type)
               << ")\n";

        for (const auto& use : description.Uses)
        {
            output << "      " << ToString(use.Access) << ' ' << GetImageName(use.Resource) << " ["
                   << ToString(use.Usage) << "]\n";
        }
        for (const auto& use : description.BufferUses)
        {
            output << "      " << ToString(use.Access) << ' ' << GetBufferName(use.Resource) << " ["
                   << ToString(use.Usage) << "]\n";
        }
    }

    output << "Dependencies:\n";
    for (const auto& dependency : result.Dependencies)
    {
        output << "  " << PassLabel(*this, dependency.From) << " -> "
               << PassLabel(*this, dependency.To);
        if (dependency.Explicit)
        {
            output << " [explicit]";
        }
        if (!dependency.Resources.empty())
        {
            output << " via ";
            for (std::size_t index = 0; index < dependency.Resources.size(); ++index)
            {
                if (index != 0)
                {
                    output << ", ";
                }
                output << GetImageName(dependency.Resources[index]);
            }
        }
        if (!dependency.BufferResources.empty())
        {
            output << (dependency.Resources.empty() ? " via " : ", ");
            for (std::size_t index = 0; index < dependency.BufferResources.size(); ++index)
            {
                if (index != 0)
                {
                    output << ", ";
                }
                output << GetBufferName(dependency.BufferResources[index]);
            }
        }
        output << '\n';
    }

    if (!result.Issues.empty())
    {
        output << "Issues:\n";
        for (const auto& issue : result.Issues)
        {
            output << "  "
                   << (issue.Level == ValidationIssue::Severity::Error ? "error: " : "warning: ")
                   << issue.Message << '\n';
        }
    }

    return output.str();
}

std::string RenderGraph::DumpDot(const CompilationResult& result) const
{
    std::ostringstream output;
    std::vector<std::uint32_t> passOrder(Passes.size(), InvalidIndex);
    for (std::uint32_t order = 0; order < result.ExecutionOrder.size(); ++order)
    {
        passOrder[result.ExecutionOrder[order].Index] = order;
    }

    const auto findLifetime = [&result](ImageHandle resource) {
        return std::find_if(
            result.ResourceLifetimes.begin(),
            result.ResourceLifetimes.end(),
            [resource](const ResourceLifetime& lifetime) { return lifetime.Resource == resource; });
    };
    const auto findBufferLifetime = [&result](BufferHandle resource) {
        return std::find_if(
            result.BufferLifetimes.begin(),
            result.BufferLifetimes.end(),
            [resource](const BufferLifetime& lifetime) { return lifetime.Resource == resource; });
    };

    output << "digraph RenderGraph {\n"
           << "  rankdir=LR;\n"
           << "  newrank=true;\n"
           << "  compound=true;\n"
           << "  splines=polyline;\n"
           << "  bgcolor=\"#10161f\";\n"
           << "  graph [fontname=\"Segoe UI\", fontsize=18, fontcolor=\"#e8eef7\", "
              "label=\"Render Graph\\nresource flow + execution dependencies\", "
              "labelloc=t, labeljust=l, pad=0.25, nodesep=0.55, ranksep=0.9];\n"
           << "  node [fontname=\"Segoe UI\", fontsize=10, color=\"#718096\", "
              "fontcolor=\"#17202b\"];\n"
           << "  edge [fontname=\"Segoe UI\", fontsize=9, color=\"#cbd5e1\", "
              "fontcolor=\"#e8eef7\", arrowsize=0.7];\n\n";

    output << "  subgraph cluster_passes {\n"
           << "    label=\"PASS ORDER / TOPOLOGICAL EXECUTION\";\n"
           << "    color=\"#405168\";\n"
           << "    fontcolor=\"#b9c7d8\";\n"
           << "    fontname=\"Segoe UI\";\n"
           << "    fontsize=11;\n"
           << "    style=\"rounded,dashed\";\n";

    for (const auto& pass : Passes)
    {
        const char* fillColor = pass.Type == PassType::Graphics ? "#b7d7c0" :
                                pass.Type == PassType::Compute ? "#b8d6ef" : "#dfbfd6";
        const char* borderColor = pass.Type == PassType::Graphics ? "#5c9b6f" :
                                  pass.Type == PassType::Compute ? "#5e91bd" : "#a76c9b";
        const auto order = passOrder[pass.Id.Index];
        const std::string orderLabel = order == InvalidIndex ? "[--]" : "[" + std::to_string(order) + "]";

        output << "    p" << pass.Id.Index
               << " [shape=box, style=\"rounded,filled\", fillcolor=\"" << fillColor
               << "\", color=\"" << borderColor << "\", penwidth=1.4, group=\"passes\", "
                  "label=\"" << EscapeDot(orderLabel) << " " << EscapeDot(pass.Name)
               << "\\n" << EscapeDot(ToString(pass.Type)) << "\", tooltip=\"pass "
               << pass.Id.Index << ": " << EscapeDot(pass.Name) << "\"];\n";
    }
    output << "  }\n\n";

    output << "  subgraph cluster_resources {\n"
           << "    label=\"IMAGES / LIFETIMES\";\n"
           << "    color=\"#665d3d\";\n"
           << "    fontcolor=\"#d9c98f\";\n"
           << "    fontname=\"Segoe UI\";\n"
           << "    fontsize=11;\n"
           << "    style=\"rounded,dashed\";\n";

    for (const auto& image : Images)
    {
        const auto lifetime = findLifetime(image.Handle);
        const bool used = lifetime != result.ResourceLifetimes.end() && lifetime->UseCount != 0;
        const std::string lifetimeLabel = used
            ? "lifetime " + std::to_string(lifetime->FirstPassOrder) + ".." +
                  std::to_string(lifetime->LastPassOrder) + " / uses " +
                  std::to_string(lifetime->UseCount)
            : "unused";
        const std::string ownershipLabel = image.Description.Imported ? "imported" : "transient";
        const char* fillColor = image.Description.Imported ? "#fff0bd" : "#d6e4f0";
        const char* borderColor = image.Description.Imported ? "#c5a94b" : "#7194ad";

        output << "    r" << image.Handle.Index
               << " [shape=box, style=\"rounded,filled\", fillcolor=\"" << fillColor
               << "\", color=\"" << borderColor << "\", penwidth=1.3, group=\"resources\", "
                  "label=\"" << EscapeDot(image.Name) << "\\n"
               << EscapeDot(ToString(image.Description.Format)) << " | "
               << image.Description.Extent.width << "x" << image.Description.Extent.height
               << " | " << EscapeDot(vk::to_string(image.Description.Usage))
               << " | samples=" << static_cast<std::uint32_t>(image.Description.Samples) << "\\n"
               << EscapeDot(ownershipLabel) << " | " << EscapeDot(lifetimeLabel)
               << "\", tooltip=\"image " << image.Handle.Index << ": "
               << EscapeDot(image.Name) << "\"];\n";
    }

    for (const auto& buffer : Buffers)
    {
        const auto lifetime = findBufferLifetime(buffer.Handle);
        const bool used = lifetime != result.BufferLifetimes.end() && lifetime->UseCount != 0;
        const std::string lifetimeLabel = used
            ? "lifetime " + std::to_string(lifetime->FirstPassOrder) + ".." +
                  std::to_string(lifetime->LastPassOrder) + " / uses " +
                  std::to_string(lifetime->UseCount)
            : "unused";
        const std::string ownershipLabel = buffer.Description.Imported ? "imported" : "transient";

        output << "    b" << buffer.Handle.Index
               << " [shape=cylinder, style=\"filled\", fillcolor=\"#c8d9ee\", "
                  "color=\"#7194ad\", penwidth=1.3, group=\"resources\", label=\""
               << EscapeDot(buffer.Name) << "\\n"
               << buffer.Description.Size << " bytes\\n"
               << EscapeDot(ownershipLabel) << " | " << EscapeDot(lifetimeLabel)
               << "\", tooltip=\"buffer " << buffer.Handle.Index << ": "
               << EscapeDot(buffer.Name) << "\"];\n";
    }
    output << "  }\n\n";

    for (const auto& pass : Passes)
    {
        for (const auto& use : pass.Uses)
        {
            const auto imageNode = "r" + std::to_string(use.Resource.Index);
            const auto passNode = "p" + std::to_string(pass.Id.Index);
            const auto label = ToString(use.Access) + " / " + ToString(use.Usage);
            const char* color = use.Access == AccessType::Read ? "#55b9e6" :
                                use.Access == AccessType::Write ? "#f0aa55" : "#c58ad9";
            const std::string attributes = "color=\"" + std::string(color) +
                "\", fontcolor=\"#e8eef7\", penwidth=1.6, label=\"" +
                EscapeDot(label) + "\"";

            if (IncludesWrite(use.Access))
            {
                output << "  " << passNode << " -> " << imageNode << " [" << attributes << "];\n";
            }
            if (IncludesRead(use.Access))
            {
                output << "  " << imageNode << " -> " << passNode << " [" << attributes << "];\n";
            }
        }
        for (const auto& use : pass.BufferUses)
        {
            const auto bufferNode = "b" + std::to_string(use.Resource.Index);
            const auto passNode = "p" + std::to_string(pass.Id.Index);
            const auto label = ToString(use.Access) + " / " + ToString(use.Usage);
            const char* color = use.Access == AccessType::Read ? "#55b9e6" :
                                use.Access == AccessType::Write ? "#f0aa55" : "#c58ad9";
            const std::string attributes = "color=\"" + std::string(color) +
                "\", fontcolor=\"#e8eef7\", penwidth=1.6, label=\"" +
                EscapeDot(label) + "\"";

            if (IncludesWrite(use.Access))
            {
                output << "  " << passNode << " -> " << bufferNode << " [" << attributes << "];\n";
            }
            if (IncludesRead(use.Access))
            {
                output << "  " << bufferNode << " -> " << passNode << " [" << attributes << "];\n";
            }
        }
    }

    for (const auto& dependency : result.Dependencies)
    {
        output << "  p" << dependency.From.Index << " -> p" << dependency.To.Index
               << " [style=dashed, color=\"#9aa8bb\", fontcolor=\"#cbd5e1\", penwidth=1.4, "
                  "constraint=false, label=\"";
        if (dependency.Explicit)
        {
            output << "explicit";
        }
        else
        {
            output << "via ";
            std::size_t labelIndex = 0;
            for (const auto resource : dependency.Resources)
            {
                if (labelIndex++ != 0)
                {
                    output << ", ";
                }
                output << EscapeDot(GetImageName(resource));
            }
            for (const auto resource : dependency.BufferResources)
            {
                if (labelIndex++ != 0)
                {
                    output << ", ";
                }
                output << EscapeDot(GetBufferName(resource));
            }
        }
        output << "\"];\n";
    }

    output << "\n"
           << "  legend [shape=note, style=\"filled\", fillcolor=\"#202b3a\", "
              "color=\"#53657c\", fontcolor=\"#e8eef7\", label=\"LEGEND\\n"
              "solid arrow = resource use\\n"
              "dashed arrow = execution dependency\\n"
              "blue = read   orange = write   purple = read/write\"];\n"
           << "}\n";
    return output.str();
}

const RenderGraph::ImageDescription& RenderGraph::GetImageDescription(ImageHandle resource) const
{
    if (!Owns(resource))
    {
        throw std::invalid_argument("RenderGraph image handle is not owned by this graph");
    }
    return Images[resource.Index].Description;
}

const std::string& RenderGraph::GetImageName(ImageHandle resource) const
{
    if (!Owns(resource))
    {
        throw std::invalid_argument("RenderGraph image handle is not owned by this graph");
    }
    return Images[resource.Index].Name;
}

const RenderGraph::BufferDescription& RenderGraph::GetBufferDescription(BufferHandle resource) const
{
    if (!Owns(resource))
    {
        throw std::invalid_argument("RenderGraph buffer handle is not owned by this graph");
    }
    return Buffers[resource.Index].Description;
}

const std::string& RenderGraph::GetBufferName(BufferHandle resource) const
{
    if (!Owns(resource))
    {
        throw std::invalid_argument("RenderGraph buffer handle is not owned by this graph");
    }
    return Buffers[resource.Index].Name;
}

const RenderGraph::PassDescription& RenderGraph::GetPassDescription(PassId pass) const
{
    if (!Owns(pass))
    {
        throw std::invalid_argument("RenderGraph pass handle is not owned by this graph");
    }
    return Passes[pass.Index];
}

bool RenderGraph::Owns(ImageHandle resource) const
{
    return resource.GraphId == GraphId && resource.Index < Images.size();
}

bool RenderGraph::Owns(BufferHandle resource) const
{
    return resource.GraphId == GraphId && resource.Index < Buffers.size();
}

bool RenderGraph::Owns(PassId pass) const
{
    return pass.GraphId == GraphId && pass.Index < Passes.size();
}

void RenderGraph::AddUse(
    PassId pass,
    ImageHandle resource,
    AccessType access,
    ImageUsage usage)
{
    if (!Owns(pass))
    {
        throw std::invalid_argument("RenderGraph image use references an unknown pass");
    }
    if (!Owns(resource))
    {
        throw std::invalid_argument("RenderGraph image use references an unknown image");
    }

    auto& uses = Passes[pass.Index].Uses;
    const auto existing = std::find_if(
        uses.begin(),
        uses.end(),
        [resource](const ImageUse& use) { return use.Resource == resource; });

    if (existing != uses.end())
    {
        if (existing->Usage != usage)
        {
            throw std::invalid_argument(
                "RenderGraph image cannot have two different usages in one pass: " +
                GetImageName(resource));
        }
        existing->Access = CombineAccess(existing->Access, access);
        return;
    }

    uses.push_back({resource, access, usage});
}

void RenderGraph::AddUse(
    PassId pass,
    BufferHandle resource,
    AccessType access,
    BufferUsage usage)
{
    if (!Owns(pass))
    {
        throw std::invalid_argument("RenderGraph buffer use references an unknown pass");
    }
    if (!Owns(resource))
    {
        throw std::invalid_argument("RenderGraph buffer use references an unknown buffer");
    }

    auto& uses = Passes[pass.Index].BufferUses;
    const auto existing = std::find_if(
        uses.begin(),
        uses.end(),
        [resource](const BufferUse& use) { return use.Resource == resource; });

    if (existing != uses.end())
    {
        if (existing->Usage != usage)
        {
            throw std::invalid_argument(
                "RenderGraph buffer cannot have two different usages in one pass: " +
                GetBufferName(resource));
        }
        existing->Access = CombineAccess(existing->Access, access);
        return;
    }

    uses.push_back({resource, access, usage});
}

RenderGraph::AccessType RenderGraph::CombineAccess(AccessType current, AccessType requested)
{
    if (current == requested)
    {
        return current;
    }
    return AccessType::ReadWrite;
}

bool RenderGraph::IncludesRead(AccessType access)
{
    return access == AccessType::Read || access == AccessType::ReadWrite;
}

bool RenderGraph::IncludesWrite(AccessType access)
{
    return access == AccessType::Write || access == AccessType::ReadWrite;
}

std::string RenderGraph::ToString(vk::Format format)
{
    return vk::to_string(format);
}

std::string RenderGraph::ToString(ImageUsage usage)
{
    switch (usage)
    {
        case ImageUsage::Sampled: return "Sampled";
        case ImageUsage::Storage: return "Storage";
        case ImageUsage::ColorAttachment: return "ColorAttachment";
        case ImageUsage::DepthAttachment: return "DepthAttachment";
        case ImageUsage::TransferSource: return "TransferSource";
        case ImageUsage::TransferDestination: return "TransferDestination";
    }
    return "UnknownUsage";
}

std::string RenderGraph::ToString(BufferUsage usage)
{
    switch (usage)
    {
        case BufferUsage::Storage: return "Storage";
        case BufferUsage::Vertex: return "Vertex";
        case BufferUsage::Index: return "Index";
        case BufferUsage::Uniform: return "Uniform";
        case BufferUsage::TransferSource: return "TransferSource";
        case BufferUsage::TransferDestination: return "TransferDestination";
    }
    return "UnknownBufferUsage";
}

std::string RenderGraph::ToString(AccessType access)
{
    switch (access)
    {
        case AccessType::Read: return "Read";
        case AccessType::Write: return "Write";
        case AccessType::ReadWrite: return "ReadWrite";
    }
    return "UnknownAccess";
}

std::string RenderGraph::ToString(PassType type)
{
    switch (type)
    {
        case PassType::Graphics: return "Graphics";
        case PassType::Compute: return "Compute";
        case PassType::Transfer: return "Transfer";
    }
    return "UnknownPassType";
}

std::string RenderGraph::EscapeDot(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value)
    {
        if (character == '\\' || character == '"')
        {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }
    return escaped;
}
