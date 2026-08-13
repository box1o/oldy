#pragma once

#include <span>

#include "render_graph.hpp"

namespace woki::gfx {

namespace graph_compile_detail {
struct State;
}

struct GraphDependency final {
    u32 before{};
    u32 after{};
    std::string reason;
};

struct GraphLifetimeInterval final {
    u32 version{};
    u32 first_use{};
    u32 last_use{};
    u32 physical_slot{kInvalidGraphIndex};
};

struct GraphTransition final {
    u32 version{};
    u32 before_pass{kInvalidGraphIndex};
    u32 pass{};
    GraphAccess before{GraphAccess::Sampled};
    GraphAccess after{GraphAccess::Sampled};
    QueueClass source_queue{QueueClass::Graphics};
    QueueClass destination_queue{QueueClass::Graphics};
    ExternalState external_state{ExternalState::Undefined};
    bool external_acquire{};
    bool external_release{};
    bool write_barrier{};
    bool queue_dependency{};
};

struct CompiledPassInfo final {
    std::string name;
    PassKind kind{PassKind::Render};
    QueueClass queue{QueueClass::Graphics};
    u32 declaration_index{};
    u32 schedule_index{};
    u32 wave{};
};

// Immutable, thread-safe metadata. Execute callbacks are owned by the graph;
// command recording is owner-thread serialized by GraphExecutor.
class CompiledRenderGraph final {
public:
    CompiledRenderGraph();
    CompiledRenderGraph(CompiledRenderGraph&&) noexcept;
    CompiledRenderGraph& operator=(CompiledRenderGraph&&) noexcept;
    CompiledRenderGraph(const CompiledRenderGraph&) = delete;
    CompiledRenderGraph& operator=(const CompiledRenderGraph&) = delete;
    ~CompiledRenderGraph();

    [[nodiscard]] std::span<const CompiledPassInfo> Passes() const noexcept;
    [[nodiscard]] std::span<const GraphDependency> Dependencies() const noexcept;
    [[nodiscard]] std::span<const GraphLifetimeInterval> Lifetimes() const noexcept;
    [[nodiscard]] std::span<const GraphTransition> Transitions() const noexcept;
    [[nodiscard]] std::span<const std::vector<u32>> Waves() const noexcept;
    [[nodiscard]] std::span<const GraphDiagnostic> Diagnostics() const noexcept;
    [[nodiscard]] u64 EstimatedTransientPeakBytes() const noexcept;
    [[nodiscard]] u64 DeterministicHash() const noexcept;
    [[nodiscard]] std::string DumpJson() const;
    [[nodiscard]] std::string DumpDot() const;
    [[nodiscard]] std::string DumpMermaid() const;

private:
    friend class GraphCompiler;
    friend class GraphExecutor;
    friend struct graph_compile_detail::State;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct GraphCompileReport final {
    std::optional<CompiledRenderGraph> graph;
    std::vector<GraphDiagnostic> diagnostics;

    [[nodiscard]] explicit operator bool() const noexcept {
        return graph.has_value();
    }
};

class GraphCompiler final {
public:
    [[nodiscard]] static Result<CompiledRenderGraph> Compile(std::shared_ptr<graph_detail::Definition> definition, u32 width, u32 height);
    [[nodiscard]] static GraphCompileReport CompileWithReport(std::shared_ptr<graph_detail::Definition> definition, u32 width, u32 height);
};

} // namespace woki::gfx
