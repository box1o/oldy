#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <type_traits>

#include "../internal/graph.hpp"
#include "../canonical_hash.hpp"

namespace woki::gfx::graph_compile_detail {

struct EdgeKey final {
    u32 before{};
    u32 after{};
    [[nodiscard]] friend auto operator<=>(const EdgeKey&, const EdgeKey&) = default;
};

struct Reader final {
    u32 pass{};
    TextureSubresourceRange range;
};

struct State final {
    std::shared_ptr<graph_detail::Definition> definition;
    u32 width{};
    u32 height{};
    std::set<std::string> names;
    std::map<EdgeKey, std::string> data_edges;
    std::vector<std::vector<Reader>> readers;
    std::vector<std::vector<u32>> reverse;
    std::vector<bool> retained;
    std::map<EdgeKey, std::string> edges;
    std::vector<u32> schedule;
    CompiledRenderGraph::Impl out;
};

[[nodiscard]] std::unexpected<Error> Failure(std::string code, std::string message);
void AddDiagnostic(std::vector<GraphDiagnostic>& diagnostics, std::string code, std::string message, std::string stage, std::string pass = {}, std::string resource = {});
[[nodiscard]] GraphDiagnostic DiagnosticFromError(const Error& error);
[[nodiscard]] bool ValidTexture(const GraphTextureDesc& value);
[[nodiscard]] QueueClass AssignQueue(const graph_detail::Pass& pass);
[[nodiscard]] bool SameExtent(const rhi::Extent3D& lhs, const rhi::Extent3D& rhs);
[[nodiscard]] const graph_detail::Use* ProducerUse(const graph_detail::Definition& definition, const graph_detail::Version& version);
[[nodiscard]] Result<void> ValidateAndBuildDependencies(State& state);
[[nodiscard]] Result<void> Cull(State& state);
[[nodiscard]] Result<void> AddRetainedDependencies(State& state);
[[nodiscard]] Result<void> BuildSchedule(State& state);
void BuildLifetimes(State& state);
void BuildTransitions(State& state);
void BuildCanonicalHash(State& state);

template <typename T>
void HashValue(u64& hash, const T& value) {
    detail::CanonicalHashWriter writer;
    writer.Value(hash);
    if constexpr (std::is_floating_point_v<T>) {
        if constexpr (sizeof(T) == sizeof(u32))
            writer.Value(std::bit_cast<u32>(value));
        else
            writer.Value(std::bit_cast<u64>(value));
    } else
        writer.Value(value);
    hash = detail::Hash64(writer.Finish());
}

void HashString(u64& hash, std::string_view value);

} // namespace woki::gfx::graph_compile_detail
