#include <iostream>
#include <string_view>

#include <woki/gfx/advanced.hpp>

using namespace woki;

int main(int argc, char** argv) {
    const std::string_view format = argc > 1 ? argv[1] : "json";
    gfx::RenderGraphBuilder builder;
    gfx::GraphTextureDesc descriptor;
    descriptor.label = "SampleColor";
    descriptor.extent = gfx::GraphExtent::Fixed(1280, 720);
    descriptor.format = rhi::TextureFormat::RGBA8Unorm;
    const auto color = builder.CreateTexture(descriptor);
    auto produce = builder.AddPass("produce", gfx::PassKind::Compute);
    const auto produced = produce.Write(color);
    produce.Queue(gfx::QueuePreference::Compute, true).Execute([](gfx::RenderGraphContext&) { return Ok(); });
    auto consume = builder.AddPass("consume", gfx::PassKind::Compute);
    consume.Read(produced).SideEffect("sample-output").Execute([](gfx::RenderGraphContext&) { return Ok(); });
    auto report = builder.CompileWithReport(1280, 720);
    if (!report) {
        for (const auto& diagnostic : report.diagnostics)
            std::cerr << diagnostic.code << " [" << diagnostic.stage << "] " << diagnostic.message << (diagnostic.pass.empty() ? "" : " pass=" + diagnostic.pass)
                      << (diagnostic.resource.empty() ? "" : " resource=" + diagnostic.resource) << '\n';
        return 1;
    }
    auto& compiled = *report.graph;
    if (format == "json")
        std::cout << compiled.DumpJson() << '\n';
    else if (format == "dot")
        std::cout << compiled.DumpDot();
    else if (format == "mermaid")
        std::cout << compiled.DumpMermaid();
    else {
        std::cerr << "usage: woki-graph [json|dot|mermaid]\n";
        return 2;
    }
    return 0;
}
