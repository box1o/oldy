#include <woki/core.hpp>
#include <woki/gfx.hpp>
#include <woki/rhi.hpp>

#include <array>

int main(int argc, char* argv[]) {
    using namespace woki;

    auto window = Window::Create({
        .title = "woki",
    });

    auto instance = TRY(rhi::Instance::Create({
        .enable_validation = true,
    }));

    auto surface = TRY(instance->CreateSurface(*window));

    auto adapter = TRY(instance->RequestAdapter({
        .compatible_surface = surface.get(),
    }));

    auto device = TRY(adapter->RequestDevice({}));

    rhi::SurfaceCapabilities capabilities{};
    TRY(surface->GetCapabilities(*adapter, capabilities));

    if (capabilities.formats.empty()) {
        return 1;
    }

    const rhi::TextureFormat color_format = capabilities.formats.front();

    auto swapchain = TRY(rhi::Swapchain::Builder(*device, *surface)
            .SizeSource(window.get())
            .ColorFormat(color_format)
            .EnableDepth(false)
            .Label("MainSwapchain")
            .Build());

    while (!window->ShouldClose()) {
        window->PollEvents();

        auto frame = TRY(swapchain->AcquireNextFrame());

        auto encoder = TRY(device->CreateCommandEncoder({
            .label = "MainCommandEncoder",
        }));

        const rhi::RenderPassColorAttachmentDesc color_attachment{
            .view = &frame.ColorView(),
            .load_op = rhi::LoadOp::Clear,
            .store_op = rhi::StoreOp::Store,
            .clear_value = { 0.1, 0.15, 0.25, 1.0, },
        };

        const rhi::RenderPassDescTyped render_pass_desc{
            .label = "ClearPass",
            .color_attachments =
                std::span<const rhi::RenderPassColorAttachmentDesc>(&color_attachment, 1),
        };

        auto render_pass = TRY(encoder->BeginRenderPass(render_pass_desc));

        render_pass->End();

        auto command_buffer = TRY(encoder->Finish({
            .label = "MainCommandBuffer",
        }));

        std::array<rhi::CommandBuffer*, 1> commands{
            command_buffer.get(),
        };

        TRY(device->GetQueue().Submit(commands));

        TRY(swapchain->Present());

        device->Tick();
        instance->ProcessEvents();
    }

    return 0;
}
