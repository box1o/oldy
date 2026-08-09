#include <array>

#include <woki/rhi.hpp>
#include <woki/core.hpp>
#include <woki/platform.hpp>

int main() {
    using namespace woki;

    auto window = TRY(Window::Create({
        .title = "woki",
    }));

    auto instance = TRY(rhi::Instance::Create());

    auto surface = TRY(instance->CreateSurface(*window));

    auto adapter = TRY(instance->RequestAdapter({
        .compatible_surface = surface.get(),
    }));

    auto device = TRY(adapter->RequestDevice());

    rhi::SurfaceCapabilities capabilities{};
    TRY(surface->GetCapabilities(*adapter, capabilities));

    if (capabilities.formats.empty()) {
        return 1;
    }

    auto swapchain = TRY(rhi::Swapchain::Builder(device, surface).Size(window->GetWidth(), window->GetHeight()).ColorFormat(capabilities.formats.front()).EnableDepth(false).Label("MainSwapchain").Build());

    while (!window->ShouldClose()) {
        window->PollEvents();

        auto frame = TRY(swapchain->AcquireNextFrame());

        auto encoder = TRY(device->CreateCommandEncoder({
            .label = "MainCommandEncoder",
        }));

        const std::array color_attachments{
            rhi::RenderPassColorAttachmentDesc{
                .view = &frame.ColorView(),
                .load_op = rhi::LoadOp::Clear,
                .store_op = rhi::StoreOp::Store,
                .clear_value =
                    {
                        0.1,
                        0.15,
                        0.25,
                        1.0,
                    },
            },
        };

        const rhi::RenderPassDescTyped render_pass_desc{
            .label = "ClearPass",
            .color_attachments = color_attachments,
        };

        auto render_pass = TRY(encoder->BeginRenderPass(render_pass_desc));

        render_pass->End();

        auto command_buffer = TRY(encoder->Finish({
            .label = "MainCommandBuffer",
        }));

        const std::array commands{
            command_buffer.get(),
        };

        TRY(device->GetQueue().Submit(commands));

        TRY(swapchain->Present());

        device->Tick();
        instance->ProcessEvents();
    }

    return 0;
}
