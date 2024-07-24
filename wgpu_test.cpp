#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <cassert>
#include <chrono>
#include <string>

#include "imgui/imgui/imgui.h"
#include "imgui_helpers.h"
#include "webgpu_helpers.h"

using namespace wgpu;

struct GPU {
    Instance instance; // XXX could avoid and use adapter.GetInstance()
    Adapter adapter; // XXX could avoid and use device.GetAdapter()
    Device device;

    void release() {
        std::cout << "Cleaning up...\n";

        device.Destroy();
    }
};

[[nodiscard]] GPU init_webgpu() {
    GPU gpu{};
    gpu.instance = CreateInstance();

    RequestAdapterOptions options = {
        .compatibleSurface = nullptr,
        .powerPreference = PowerPreference::HighPerformance,
        .forceFallbackAdapter = false,
        .compatibilityMode = false,
    };
    FutureWaitInfo adapter_future;
    adapter_future.future = gpu.instance.RequestAdapter(&options, CallbackMode::WaitAnyOnly,
        [&gpu] (RequestAdapterStatus status, Adapter adapter, char const* message) {
            assert(status == RequestAdapterStatus::Success);
            gpu.adapter = adapter;
    });

    auto status = gpu.instance.WaitAny(1, &adapter_future, 0);
    if (status == WaitStatus::Success && adapter_future.completed) {
        DeviceDescriptor options;
        options.deviceLostCallbackInfo = {
                .callback = [](WGPUDevice const * device, WGPUDeviceLostReason reason, char const * message, void *) {
                    std::cout << "Device Lost: " << message << "\n";
                },
        };
        options.uncapturedErrorCallbackInfo = {
                .callback = [](WGPUErrorType type, char const * message, void *) {
                    std::cout << "Error: " << message << "\n";
                },
        };
        FutureWaitInfo device_future;
        device_future.future = gpu.adapter.RequestDevice(&options, CallbackMode::WaitAnyOnly,
            [&gpu](RequestDeviceStatus status, Device device, char const* msg) {
                assert(status == RequestDeviceStatus::Success);
                gpu.device = device;
            });
        auto status = gpu.instance.WaitAny(1, &device_future, 0);
        if (status == WaitStatus::Success && device_future.completed) {
        }
    }

    return gpu;
}

static bool show_imgui_demo = false;

static const char shader_code[] = R"WGSL(
    @vertex fn vertexMain(@builtin(vertex_index) i : u32) ->
      @builtin(position) vec4f {
        const pos = array(vec2f(0, 1), vec2f(-1, -1), vec2f(1, -1));
        return vec4f(pos[i], 0, 1);
    }

    @fragment fn fragmentMain() -> @location(0) vec4f {
        return vec4f(1, 0, 0, 1);
    }
)WGSL";


int test_main_replace() {

    // auto gpu = init_webgpu();

    // ImGuiWebGPU imgui(gpu.device);

    // ShaderModuleWGSLDescriptor wgsl_desc;
    // wgsl_desc.code = shader_code;

    // ShaderModuleDescriptor shader_desc{
    //     .nextInChain = &wgsl_desc,
    // };

    // ShaderModule shader_module = gpu.device.CreateShaderModule(&shader_desc);

    // ColorTargetState color_target_state{
    //     .format = TextureFormat::BGRA8Unorm
    // };

    // FragmentState fragment_state{
    //     .module = shader_module,
    //     .targetCount = 1,
    //     .targets = &color_target_state
    // };

    // RenderPipelineDescriptor descriptor{
    //     .vertex = { .module = shader_module },
    //     .fragment = &fragment_state
    // };
    // RenderPipeline pipeline = gpu.device.CreateRenderPipeline(&descriptor);

    // int width, height;

    // TextureCapture capture(gpu.device);

    // bool is_rendering = false;
    // while (is_rendering) {
    //     std::vector<CommandBuffer> commands;

        // RenderPassColorAttachment attachment{
        //     .view = surface_info.texture.CreateView(),
        //     .loadOp = LoadOp::Clear,
        //     .storeOp = StoreOp::Store
        // };

        // RenderPassDescriptor render_pass{
        //     .colorAttachmentCount = 1,
        //     .colorAttachments = &attachment
        // };

        // CommandEncoder encoder = gpu.device.CreateCommandEncoder();
        // RenderPassEncoder pass = encoder.BeginRenderPass(&render_pass);
        // pass.SetPipeline(pipeline);
        // pass.Draw(3);
        // pass.End();

        // commands.push_back(encoder.Finish());
        // gpu.device.GetQueue().Submit(commands.size(), commands.data());

        // imgui.begin_frame(surface_info.texture.GetWidth(), surface_info.texture.GetHeight());

        // // simple fps counter
        // static int frame_count = 0;
        // static auto prev = std::chrono::high_resolution_clock::now();
        // static float mean_fps = 0.0;
        // if (frame_count >= 60) {
        //     auto now = std::chrono::high_resolution_clock::now();
        //     std::chrono::duration<double, std::micro> mean_us = now - prev;
        //     mean_fps = float(frame_count) / (mean_us.count() * 1e-6);
        //     prev = now;
        //     frame_count = 1;
        // } else {
        //     frame_count++;
        // }

        // ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        // window_flags |= ImGuiWindowFlags_NoMove;

        // const float PAD = 10.0f;
        // const ImGuiViewport* viewport = ImGui::GetMainViewport();
        // ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
        // ImVec2 work_size = viewport->WorkSize;
        // ImVec2 window_pos, window_pos_pivot;
        // window_pos.x = work_pos.x + PAD;
        // window_pos.y = work_pos.y + PAD;
        // window_pos_pivot.x = 0.0f;
        // window_pos_pivot.y = 0.0f;
        // ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);

        // static std::stringstream capture_file_name;
        // static int capture_seq = 0;
        // static int capture_frame = 0;
        // static bool do_capture = false;
        // static bool capture_include_ui = false;
        // if (ImGui::Begin("Example: Simple overlay", nullptr, window_flags)) {
        //     ImGui::Text("[F1] Open/Close demo window");
        //     ImGui::Text("Fps: %.1f", mean_fps);
        //     const char * capture_btn_labels[2] = { "Screen capture", "Stop capture" };
        //     const ImVec4 capture_btn_colors[2] = { ImGui::GetStyleColorVec4(ImGuiCol_Button), (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f) };
        //     const ImVec4 capture_btn_hovered_colors[2] = { ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.8f) };
        //     const ImVec4 capture_btn_active_colors[2] = { ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive), (ImVec4)ImColor::HSV(0.0f, 0.6f, 1.0f) };
        //     ImGui::PushStyleColor(ImGuiCol_Button, capture_btn_colors[do_capture]);
        //     ImGui::PushStyleColor(ImGuiCol_ButtonHovered, capture_btn_hovered_colors[do_capture]);
        //     ImGui::PushStyleColor(ImGuiCol_ButtonActive, capture_btn_active_colors[do_capture]);
        //     if (ImGui::Button(capture_btn_labels[do_capture])) {
        //         do_capture = !do_capture;
        //         if (do_capture) {
        //             capture_seq = (capture_seq + 1) % 100;
        //             capture_frame = 0;
        //         }
        //     }
        //     ImGui::PopStyleColor(3);
        //     ImGui::SameLine();
        //     ImGui::Checkbox("/w UI", &capture_include_ui);

        //     if (do_capture) {
        //         capture_file_name.str("");
        //         capture_file_name << "capture_s" << std::setfill('0') << std::setw(2) << capture_seq
        //             << "_" << std::setw(5) << capture_frame << ".ppm";
        //         ImGui::Text("%s", capture_file_name.str().c_str());
        //         capture_frame = (capture_frame + 1) % 10000;
        //     }
        // }
        // ImGui::End();

        // if (show_imgui_demo) {
        //     ImGui::ShowDemoWindow(&show_imgui_demo);
        // }

        // if (capture_include_ui) {
        //     imgui.end_frame(surface_info.texture);
        // }

        // if (do_capture) {
        //     std::string file_name = capture_file_name.str();
        //     capture.push(surface_info.texture, nullptr,
        //         [file_name](char const * image_data, ImageDataLayout const& layout) {
        //         std::ofstream file(file_name);
        //         file << "P6\n" << layout.width << " " << layout.height << "\n" << 255 << "\n";
        //         for (int row = 0; row <layout.height; row++) {
        //                 char const * pixel = image_data + row * layout.row_stride;
        //                 for (int col = 0; col <layout.width; col++, pixel += 4) {
        //                     if (layout.format == TextureFormat::BGRA8Unorm) {
        //                         char rgb[3] = { *(pixel + 2), *(pixel + 1), *pixel };
        //                         file.write(rgb, 3);
        //                     } else {
        //                         file.write(pixel, 3);
        //                     }
        //             }
        //         }
        //     });
        // }

        // if (!capture_include_ui) {
        //     imgui.end_frame(surface_info.texture);
        // }

        // surface.Present();

        // // Poll for and process events
        // gpu.instance.ProcessEvents();

        // if (do_capture) {
        //     capture.pop();
        // }
    // }

    // gpu.release();

    return 0;
}
