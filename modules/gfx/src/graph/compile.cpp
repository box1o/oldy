#include "internal.hpp"

namespace woki::gfx {

using namespace graph_compile_detail;

CompiledRenderGraph::CompiledRenderGraph()
    : impl_(std::make_unique<Impl>()) {}

CompiledRenderGraph::CompiledRenderGraph(CompiledRenderGraph&&) noexcept = default;
CompiledRenderGraph& CompiledRenderGraph::operator=(CompiledRenderGraph&&) noexcept = default;
CompiledRenderGraph::~CompiledRenderGraph() = default;

u64 CompiledRenderGraph::EstimatedTransientPeakBytes() const noexcept {
    return impl_->peak_bytes;
}

u64 CompiledRenderGraph::DeterministicHash() const noexcept {
    return impl_->hash;
}

Result<CompiledRenderGraph> GraphCompiler::Compile(
    std::shared_ptr<graph_detail::Definition> definition,
    const u32 width,
    const u32 height
) {
    if (definition == nullptr || width == 0 || height == 0)
        return Failure("GRF1000", "compile requires a definition and non-zero frame extent");

    State state;
    state.definition = std::move(definition);
    state.width = width;
    state.height = height;
    TRY_VOID(ValidateAndBuildDependencies(state));
    TRY_VOID(Cull(state));
    TRY_VOID(AddRetainedDependencies(state));
    TRY_VOID(BuildSchedule(state));
    BuildLifetimes(state);
    BuildTransitions(state);
    BuildCanonicalHash(state);

    CompiledRenderGraph result;
    *result.impl_ = std::move(state.out);
    return Ok(std::move(result));
}

} // namespace woki::gfx
