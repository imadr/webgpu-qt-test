#pragma once

#include <string>
#include <vector>

#include <imgui/imgui.h>
#include <webgpu/webgpu_cpp.h>

class ImGuiWebGPU {
public:
    ImGuiWebGPU(wgpu::Device device);

    ~ImGuiWebGPU();

    void begin_frame(uint32_t win_width, uint32_t win_height);

    void end_frame(wgpu::Texture target);

private:
    ImGuiContext *context;
    wgpu::Device device;
    wgpu::RenderPipeline pipeline;
    wgpu::BindGroup bind_groups[3];
    wgpu::Buffer view_uniform_buffer;
    wgpu::Buffer draw_flags_uniform_buffer;
    wgpu::Texture font_texture;
    uint32_t last_draw_flags;
    wgpu::Texture* last_texture;
};

class ImGuiGlfw {
public:
    ~ImGuiGlfw();

    bool is_idle();

private:
    int refresh_frame_count = 0;
};
