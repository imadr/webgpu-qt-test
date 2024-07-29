#include "imgui_helpers.h"

/* #include <algorithm> */
/* #include <unordered_map> */
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "webgpu_helpers.h"

using namespace wgpu;

/* using namespace UI; */

/* std::string const ImGuiHelper::s_fontTextureName = "_imGuiFont"; */
/* std::string const ImGuiHelper::s_baseMeshName    = "_imGuiMesh_"; */

/* ImGuiKey convertToImGuiKey(UI::Key::Enum key); */

ImGuiWebGPU::ImGuiWebGPU(Device device) {
    this->context = ImGui::CreateContext();
    this->device = device;

    ImGuiIO& io = ImGui::GetIO();

    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.IniFilename = nullptr;

    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsClassic(&style);
    style.FrameRounding = 4.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowPadding = ImVec2(10.0f, 10.0f);

    style.AntiAliasedLines = true;
    style.AntiAliasedLinesUseTex = false;

    // Create pipeline
    VertexAttribute attributes[3] = {
        {.format = VertexFormat::Float32x2, .offset = 0, .shaderLocation = 0},
        {.format = VertexFormat::Float32x2, .offset = 8, .shaderLocation = 1},
        {.format = VertexFormat::Unorm8x4, .offset = 16, .shaderLocation = 2},
    };
    VertexBufferLayout vertex_layout{
        .arrayStride = sizeof(ImDrawVert),
        .attributeCount = 3,
        .attributes = attributes,
    };

    const char* ui_source = R"WGSL(
        struct Vertex {
            @location(0) ss_pos: vec2f,
            @location(1) uv: vec2f,
            @location(2) color: vec4f,
        };

        struct VertexOutput {
            @builtin(position) position: vec4f,
            @location(0) uv: vec2f,
            @location(1) color: vec4f,
        };

        struct DrawInfo {
            tex_is_srgb: u32,
            target_is_srgb: u32,
        };

        // (width, height, 1.0/width, 1.0/height)
        @group(0) @binding(0) var<uniform> view_info: vec4f;
        // draw flags: bit 0: texture_is_srgb
        //             bit 1: target_is_srgb
        @group(0) @binding(1) var<uniform> draw_flags : u32;

        @vertex fn vertexMain(vertex: Vertex) -> VertexOutput {
            var out: VertexOutput;

            var cs_pos = 2.0 * (vertex.ss_pos * view_info.zw) - 1.0;
            cs_pos.y = -cs_pos.y;
            out.position = vec4f(cs_pos, 0.0, 1.0);
            out.uv = vertex.uv;
            out.color = vertex.color;

            return out;
        }

        @group(0) @binding(2) var draw_sampler: sampler;
        @group(1) @binding(0) var draw_texture: texture_2d<f32>;

        struct FragmentInput {
            @builtin(position) position: vec4f,
            @location(0) uv: vec2f,
            @location(1) color: vec4f,
        };

        @fragment fn fragmentMain(fragment: FragmentInput) -> @location(0) vec4f {
            var texture_is_srgb: bool = bool(draw_flags & 0x1);
            var target_is_srgb: bool = bool(draw_flags & 0x2);
            var texel = textureSample(draw_texture, draw_sampler, fragment.uv);
            return fragment.color * texel;
        }
    )WGSL";

    ShaderModuleWGSLDescriptor shader_desc;
    shader_desc.code = ui_source;
    ShaderModuleDescriptor module_desc{
        .nextInChain = &shader_desc,
        .label = "ui",
    };
    ShaderModule module = device.CreateShaderModule(&module_desc);
    FutureWaitInfo build_log_future;
    build_log_future.future = module.GetCompilationInfo(
        CallbackMode::WaitAnyOnly,
        [ui_source](CompilationInfoRequestStatus status, CompilationInfo const* info) {
            /* if (status == CompilationInfoRequestStatus::Success && info->messageCount > 0) { */
            /*     std::cout << "WGSL Compilation log:\n"; */
            /*     static const char* msg_types[4] = { "", "Error", "Warning", "Info" }; */
            /*     for (int i = 0; i < info->messageCount; i++) { */
            /*         std::string_view excerpt(ui_source + info->messages[i].offset,
             * info->messages[i].length); */
            /*         std::cout */
            /*         << "[" << msg_types[static_cast<uint32_t>(info->messages[i].type)] << "] " */
            /*         << info->messages[i].lineNum << ": " << info->messages[i].message << "\n" */
            /*         << excerpt << "\n"; */
            /*     } */
            /* } */
        });

    WaitStatus status = device.GetAdapter().GetInstance().WaitAny(1, &build_log_future, 0);
    if (status != WaitStatus::Success || !build_log_future.completed) {
        std::cout << "Shader compilation error\n";
        // TODO roll back
    }

    /* DepthStencilState depth_stenci_state{ */
    /*     .format = TextureFormat::DepthStencilState */
    /* Bool depthWriteEnabled = false; */
    /* CompareFunction depthCompare = CompareFunction::Undefined; */
    /* StencilFaceState stencilFront; */
    /* StencilFaceState stencilBack; */
    /* }; */
    BlendState blend_state{
        .color = {.srcFactor = BlendFactor::SrcAlpha, .dstFactor = BlendFactor::OneMinusSrcAlpha},
    };
    ColorTargetState target{
        .format = TextureFormat::BGRA8Unorm,
        .blend = &blend_state,
    };
    FragmentState fragment_state{
        .module = module,
        .entryPoint = "fragmentMain",
        .targetCount = 1,
        .targets = &target,
    };
    RenderPipelineDescriptor pipeline_desc{
        .label = "ui",
        .vertex =
            {
                .module = module,
                .entryPoint = "vertexMain",
                .bufferCount = 1,
                .buffers = &vertex_layout,
            },
        .fragment = &fragment_state,
    };
    this->pipeline = device.CreateRenderPipeline(&pipeline_desc);

    // view info uniform buffer
    BufferDescriptor view_uniform_desc{
        .usage = BufferUsage::Uniform | BufferUsage::CopyDst,
        .size = 16,
    };
    this->view_uniform_buffer = device.CreateBuffer(&view_uniform_desc);

    // draw flags uniform buffer
    BufferDescriptor draw_flags_uniform_desc{
        .usage = BufferUsage::Uniform | BufferUsage::CopyDst,
        .size = 4,
    };
    this->draw_flags_uniform_buffer = device.CreateBuffer(&draw_flags_uniform_desc);
    uint32_t flags = 0;
    device.GetQueue().WriteBuffer(this->draw_flags_uniform_buffer, 0, &flags, 4);

    BindGroupEntry pass_bindings[3] = {
        {.binding = 0, .buffer = this->view_uniform_buffer},
        {.binding = 1, .buffer = this->draw_flags_uniform_buffer},
        {.binding = 2, .sampler = device.CreateSampler()},
    };
    BindGroupDescriptor bind_group_desc{
        .layout = this->pipeline.GetBindGroupLayout(0),
        .entryCount = 3,
        .entries = pass_bindings,
    };
    this->bind_groups[0] = device.CreateBindGroup(&bind_group_desc);

    // Font glyphs texture atlas
    uint8_t* data;
    int32_t w, h;
    io.Fonts->GetTexDataAsRGBA32(&data, &w, &h);
    uint32_t font_tex_width = static_cast<uint32_t>(w);
    uint32_t font_tex_height = static_cast<uint32_t>(h);
    size_t size = font_tex_width * font_tex_height * 4;

    TextureDescriptor font_tex_desc{
        .label = "ImGui font texture",
        .usage = TextureUsage::TextureBinding | TextureUsage::CopyDst,
        .size = {.width = font_tex_width, .height = font_tex_height},
        //.format = TextureFormat::BGRA8Unorm,
        .format = TextureFormat::RGBA8Unorm,
    };

    this->font_texture = device.CreateTexture(&font_tex_desc);
    ImageCopyTexture dest{
        .texture = this->font_texture,
    };
    Extent3D write_size{
        .width = font_tex_width,
        .height = font_tex_height,
    };
    TextureDataLayout layout{
        .bytesPerRow = font_tex_width * 4,
    };
    device.GetQueue().WriteTexture(&dest, data, size, &layout, &write_size);
    io.Fonts->SetTexID((void*)&font_texture);

    // font texture bind group
    BindGroupEntry font_texture_entry{
        .binding = 0,
        .textureView = this->font_texture.CreateView(),
    };
    bind_group_desc = {
        .layout = this->pipeline.GetBindGroupLayout(1),
        .entryCount = 1,
        .entries = &font_texture_entry,
    };
    this->bind_groups[1] = device.CreateBindGroup(&bind_group_desc);
    this->last_texture = &this->font_texture;
}

ImGuiWebGPU::~ImGuiWebGPU() {
    if (context) {
        ImGui::DestroyContext(context);
    }
}

void ImGuiWebGPU::begin_frame(uint32_t win_width, uint32_t win_height) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(win_width, win_height);

    float view_info[4] = {io.DisplaySize.x, io.DisplaySize.y, 1.0f / io.DisplaySize.x,
                          1.0f / io.DisplaySize.y};
    this->device.GetQueue().WriteBuffer(this->view_uniform_buffer, 0, view_info, 16);

    ImGui::NewFrame();
}

void ImGuiWebGPU::end_frame(Texture target) {
    ImGui::Render();

    uint32_t draw_flags = is_srgb(target.GetFormat()) << 1 | this->last_draw_flags & 0x1;

    ImDrawData* draw_data = ImGui::GetDrawData();

    CommandEncoder encoder = this->device.CreateCommandEncoder();

    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* commands = draw_data->CmdLists[n];
        const ImDrawVert* ui_vertices = commands->VtxBuffer.Data;
        const ImDrawIdx* ui_indices = commands->IdxBuffer.Data;

        uint32_t num_vertices = (uint32_t)commands->VtxBuffer.size();

        BufferDescriptor vb_desc{
            .label = "ui vertex buffer",
            .usage = BufferUsage::Vertex | BufferUsage::CopyDst,
            .size =
                sizeof(ImDrawVert) * num_vertices,  // note: ImDrawVert is already a multiple of 4
        };
        Buffer v_buffer = this->device.CreateBuffer(&vb_desc);

        this->device.GetQueue().WriteBuffer(v_buffer, 0, ui_vertices, vb_desc.size);

        uint32_t size = commands->IdxBuffer.size_in_bytes();
        BufferDescriptor ib_desc{
            .label = "ui index buffer",
            .usage = BufferUsage::Index | BufferUsage::CopyDst,
            .size = align_up(size, 4),
        };
        Buffer i_buffer = this->device.CreateBuffer(&ib_desc);

        if (commands->IdxBuffer.capacity() * sizeof(uint16_t) >= ib_desc.size) {
            // use extra capacity to write up to alignment
            this->device.GetQueue().WriteBuffer(i_buffer, 0, ui_indices, ib_desc.size);
        } else {
            uint32_t aligned_size = align_up(size - 4, 4);
            this->device.GetQueue().WriteBuffer(i_buffer, 0, ui_indices, aligned_size);
            uint8_t rest[4];
            memcpy(rest, ((uint8_t*)ui_indices) + aligned_size, size - aligned_size);
            this->device.GetQueue().WriteBuffer(i_buffer, aligned_size, rest, 4);
        }

        // std::cout << "Draw list #" << n << ": " << num_vertices << " vertices, "
        //     << commands->IdxBuffer.size() << " indices\n";

        RenderPassColorAttachment colorAttachment{
            .view = target.CreateView(),
            .loadOp = LoadOp::Load,
            .storeOp = StoreOp::Store,
        };
        RenderPassDescriptor pass_desc{
            .colorAttachmentCount = 1,
            .colorAttachments = &colorAttachment,
        };
        RenderPassEncoder pass = encoder.BeginRenderPass(&pass_desc);

        pass.SetPipeline(this->pipeline);
        pass.SetVertexBuffer(0, v_buffer);
        pass.SetIndexBuffer(i_buffer, IndexFormat::Uint16);
        pass.SetBindGroup(0, this->bind_groups[0]);
        pass.SetBindGroup(1, this->bind_groups[1]);

        for (int cmd = 0; cmd < commands->CmdBuffer.Size; cmd++) {
            const ImDrawCmd* pcmd = &commands->CmdBuffer[cmd];
            if (pcmd->ElemCount == 0) {
                continue;
            }

            if (pcmd->UserCallback) {
                pcmd->UserCallback(commands, pcmd);
            } else {
                pass.SetScissorRect(static_cast<uint32_t>(pcmd->ClipRect.x),
                                    static_cast<uint32_t>(pcmd->ClipRect.y),
                                    static_cast<uint32_t>(pcmd->ClipRect.z - pcmd->ClipRect.x),
                                    static_cast<uint32_t>(pcmd->ClipRect.w - pcmd->ClipRect.y));

                // TODO: lower to WGPUTexture type and use Acquire/release ?
                Texture* texture = static_cast<Texture*>(pcmd->TextureId);
                if (texture->Get() != this->last_texture->Get()) {
                    if (texture->Get() == this->font_texture.Get()) {
                        pass.SetBindGroup(1, this->bind_groups[1]);
                    } else {
                        BindGroupEntry texture_entry{
                            .binding = 0,
                            .textureView = texture->CreateView(),
                        };
                        BindGroupDescriptor bind_group_desc{
                            .layout = this->pipeline.GetBindGroupLayout(1),
                            .entryCount = 1,
                            .entries = &texture_entry,
                        };
                        this->bind_groups[2] = device.CreateBindGroup(&bind_group_desc);
                        pass.SetBindGroup(1, this->bind_groups[2]);
                    }
                    this->last_texture = texture;
                }

                draw_flags |= is_srgb(texture->GetFormat()) & 0x1;
                if (draw_flags != this->last_draw_flags) {
                    this->device.GetQueue().WriteBuffer(this->draw_flags_uniform_buffer, 0,
                                                        &draw_flags, 4);
                    this->last_draw_flags = draw_flags;
                }

                pass.DrawIndexed(pcmd->ElemCount, 1, pcmd->IdxOffset, pcmd->VtxOffset);
            }
        }
        pass.End();
    }

    CommandBuffer cmd_buffer = encoder.Finish();
    this->device.GetQueue().Submit(1, &cmd_buffer);
}
