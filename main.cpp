#include <QtQuick/qquickwindow.h>

#include <QApplication>
#include <QGuiApplication>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QRunnable>
#include <QTimer>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLTexture>
#include <thread>

///// webgpu stuff

#include <imgui/imgui.h>

#include <cassert>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

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


int webgpu_main() {
  

    auto gpu = init_webgpu();

    ImGuiWebGPU imgui(gpu.device);


    ShaderModuleWGSLDescriptor wgsl_desc;
    wgsl_desc.code = shader_code;

    ShaderModuleDescriptor shader_desc{
        .nextInChain = &wgsl_desc,
    };

    ShaderModule shader_module = gpu.device.CreateShaderModule(&shader_desc);

    ColorTargetState color_target_state{
        .format = TextureFormat::BGRA8Unorm
    };

    FragmentState fragment_state{
        .module = shader_module,
        .targetCount = 1,
        .targets = &color_target_state
    };

    RenderPipelineDescriptor descriptor{
        .vertex = { .module = shader_module },
        .fragment = &fragment_state
    };
    RenderPipeline pipeline = gpu.device.CreateRenderPipeline(&descriptor);

    int width, height;
width = 800;
height = 600;
    TextureCapture capture(gpu.device);
    
    TextureDescriptor textureDesc;
    textureDesc.size.width = 1024; 
    textureDesc.size.height = 768;
    textureDesc.size.depthOrArrayLayers = 1;
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.dimension = TextureDimension::e2D;
    textureDesc.format = TextureFormat::BGRA8Unorm;
    textureDesc.usage = TextureUsage::RenderAttachment | TextureUsage::CopySrc;

Texture texture = gpu.device.CreateTexture(&textureDesc);

TextureView textureView = texture.CreateView();
bool keep_rendering = true;
    while (keep_rendering) {
        std::vector<CommandBuffer> commands;


        RenderPassColorAttachment attachment{
            .view = textureView,
            .loadOp = LoadOp::Clear,
            .storeOp = StoreOp::Store
        };

        RenderPassDescriptor render_pass{
            .colorAttachmentCount = 1,
            .colorAttachments = &attachment
        };

        CommandEncoder encoder = gpu.device.CreateCommandEncoder();
        RenderPassEncoder pass = encoder.BeginRenderPass(&render_pass);
        pass.SetPipeline(pipeline);
        pass.Draw(3);
        pass.End();

        commands.push_back(encoder.Finish());
        gpu.device.GetQueue().Submit(commands.size(), commands.data());

        imgui.begin_frame(texture.GetWidth(), texture.GetHeight());

        // simple fps counter
        static int frame_count = 0;
        static auto prev = std::chrono::high_resolution_clock::now();
        static float mean_fps = 0.0;
        if (frame_count >= 60) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::micro> mean_us = now - prev;
            mean_fps = float(frame_count) / (mean_us.count() * 1e-6);
            prev = now;
            frame_count = 1;
        } else {
            frame_count++;
        }

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        window_flags |= ImGuiWindowFlags_NoMove;

        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = work_pos.x + PAD;
        window_pos.y = work_pos.y + PAD;
        window_pos_pivot.x = 0.0f;
        window_pos_pivot.y = 0.0f;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);

        static std::stringstream capture_file_name;
        static int capture_seq = 0;
        static int capture_frame = 0;
        static bool do_capture = true;
        static bool capture_include_ui = false;
        if (ImGui::Begin("Example: Simple overlay", nullptr, window_flags)) {
            ImGui::Text("[F1] Open/Close demo window");
            ImGui::Text("Fps: %.1f", mean_fps);
            const char * capture_btn_labels[2] = { "Screen capture", "Stop capture" };
            const ImVec4 capture_btn_colors[2] = { ImGui::GetStyleColorVec4(ImGuiCol_Button), (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f) };
            const ImVec4 capture_btn_hovered_colors[2] = { ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.8f) };
            const ImVec4 capture_btn_active_colors[2] = { ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive), (ImVec4)ImColor::HSV(0.0f, 0.6f, 1.0f) };
            ImGui::PushStyleColor(ImGuiCol_Button, capture_btn_colors[do_capture]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, capture_btn_hovered_colors[do_capture]);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, capture_btn_active_colors[do_capture]);
            if (ImGui::Button(capture_btn_labels[do_capture])) {
                do_capture = !do_capture;
                if (do_capture) {
                    capture_seq = (capture_seq + 1) % 100;
                    capture_frame = 0;
                }
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            ImGui::Checkbox("/w UI", &capture_include_ui);

            if (do_capture) {
                capture_file_name.str("");
                capture_file_name << "capture_s" << std::setfill('0') << std::setw(2) << capture_seq
                    << "_" << std::setw(5) << capture_frame << ".ppm";
                ImGui::Text("%s", capture_file_name.str().c_str());
                capture_frame = (capture_frame + 1) % 10000;
            }
        }
        ImGui::End();

        if (capture_include_ui) {
            imgui.end_frame(texture);
        }

        if (do_capture) {
            std::string file_name = capture_file_name.str();
            capture.push(texture, nullptr,
                [file_name](char const * image_data, ImageDataLayout const& layout) {
                std::ofstream file(file_name);
                file << "P6\n" << layout.width << " " << layout.height << "\n" << 255 << "\n";
                for (int row = 0; row <layout.height; row++) {
                        char const * pixel = image_data + row * layout.row_stride;
                        for (int col = 0; col <layout.width; col++, pixel += 4) {
                            if (layout.format == TextureFormat::BGRA8Unorm) {
                                char rgb[3] = { *(pixel + 2), *(pixel + 1), *pixel };
                                file.write(rgb, 3);
                            } else {
                                file.write(pixel, 3);
                            }
                    }
                }
            });
        }

        if (!capture_include_ui) {
            imgui.end_frame(texture);
        }

        // Poll for and process events
        gpu.instance.ProcessEvents();

        if (do_capture) {
            capture.pop();
        }

    }

    gpu.release();

    return 0;
}
///// webgpu stuff

class TextureCanvasRenderer : public QObject, protected QOpenGLFunctions {
    Q_OBJECT
   public:
    TextureCanvasRenderer() : m_t(0), m_program(0) {
    }
    ~TextureCanvasRenderer() {
        delete m_program;
    }

    void setT(qreal t) {
        m_t = t;
    }
    void setViewportSize(const QSize &size) {
        m_viewportSize = size;
    }
    void setWindow(QQuickWindow *window) {
        m_window = window;
    }

    void updateTextureData() {
        if (m_texture != NULL) {
            m_texture->destroy();
            m_texture = NULL;
        }
        if (!m_image->isNull()) {
            m_texture = new QOpenGLTexture(m_image->mirrored());
            m_texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
            m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
            m_texture->setWrapMode(QOpenGLTexture::Repeat);
        }
    }

   public slots:
    void init() {
        if (!m_program) {
            QSGRendererInterface *rif = m_window->rendererInterface();
            Q_ASSERT(rif->graphicsApi() == QSGRendererInterface::OpenGL ||
                     rif->graphicsApi() == QSGRendererInterface::OpenGLRhi);

            initializeOpenGLFunctions();

            m_program = new QOpenGLShaderProgram();
            m_program->addCacheableShaderFromSourceCode(
                QOpenGLShader::Vertex,
                "varying highp vec2 coords;"
                "void main() {"
                "vec3 position = vec3(vec2(gl_VertexID % 2, gl_VertexID / 2) * 4.0 - 1.0, 0.0);"
                "coords = (position.xy + 1.0) * 0.5;"
                "gl_Position = vec4(position, 1.0);"
                "}");
            m_program->addCacheableShaderFromSourceCode(
                QOpenGLShader::Fragment,
                "uniform lowp float t;"
                "uniform sampler2D texture;"
                "varying highp vec2 coords;"
                "void main() {"
                "    gl_FragColor = texture2D(texture, coords);"
                "}");

            m_program->link();

            m_image = new QImage(400, 400, QImage::Format_RGBA8888);
            m_image->fill(Qt::red);
            QPainter painter(m_image);
            painter.setPen(Qt::blue);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(25, 25, 50, 50);
            painter.end();

            updateTextureData();
        }
    }
    void paint() {
        m_window->beginExternalCommands();

        m_program->bind();

        m_program->enableAttributeArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        m_program->setUniformValue("t", (float)m_t);

        glViewport(0, 0, m_viewportSize.width(), m_viewportSize.height());

        glDisable(GL_DEPTH_TEST);

        if (m_texture != NULL) {
            m_texture->bind();
        }
        glDrawArrays(GL_TRIANGLES, 0, 3);

        m_program->disableAttributeArray(0);
        m_program->release();

        m_window->resetOpenGLState();

        m_window->endExternalCommands();
    }

   private:
    QSize m_viewportSize;
    qreal m_t;
    QOpenGLShaderProgram *m_program;
    QQuickWindow *m_window;
    QImage *m_image;
    QOpenGLTexture *m_texture = NULL;
};

class CleanupJob : public QRunnable {
   public:
    CleanupJob(TextureCanvasRenderer *renderer) : m_renderer(renderer) {
    }
    void run() override {
        delete m_renderer;
    }

   private:
    TextureCanvasRenderer *m_renderer;
};

class TextureCanvas : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(qreal t READ t WRITE setT NOTIFY tChanged)
    QML_ELEMENT

   public:
    TextureCanvas() : m_t(0), m_renderer(nullptr) {
        connect(this, &QQuickItem::windowChanged, this, &TextureCanvas::handleWindowChanged);
    }

    qreal t() const {
        return m_t;
    }
    void setT(qreal t) {
        if (t == m_t) return;
        m_t = t;
        emit tChanged();
        if (window()) window()->update();
    }

   signals:
    void tChanged();

   public slots:
    void sync() {
        if (!m_renderer) {
            m_renderer = new TextureCanvasRenderer();
            connect(window(), &QQuickWindow::beforeRendering, m_renderer,
                    &TextureCanvasRenderer::init, Qt::DirectConnection);
            connect(window(), &QQuickWindow::beforeRenderPassRecording, m_renderer,
                    &TextureCanvasRenderer::paint, Qt::DirectConnection);
        }
        m_renderer->setViewportSize(window()->size() * window()->devicePixelRatio());
        m_renderer->setT(m_t);
        m_renderer->setWindow(window());
    }
    void cleanup() {
        delete m_renderer;
        m_renderer = nullptr;
    }

   private slots:
    void handleWindowChanged(QQuickWindow *win) {
        if (win) {
            connect(win, &QQuickWindow::beforeSynchronizing, this, &TextureCanvas::sync,
                    Qt::DirectConnection);
            connect(win, &QQuickWindow::sceneGraphInvalidated, this, &TextureCanvas::cleanup,
                    Qt::DirectConnection);
            win->setColor(Qt::black);
        }
    }

   private:
    void releaseResources() {
        window()->scheduleRenderJob(new CleanupJob(m_renderer),
                                    QQuickWindow::BeforeSynchronizingStage);
        m_renderer = nullptr;
    }

    qreal m_t;
    TextureCanvasRenderer *m_renderer;
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    std::thread t(webgpu_main);

    QQmlApplicationEngine engine;

    qmlRegisterType<TextureCanvas>("CustomComponents", 1, 0, "TextureCanvas");

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    return app.exec();
}

#include "main.moc"