#include <QtQuick/qquickwindow.h>

#include <QApplication>
#include <QGuiApplication>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QOpenGLFunctions>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QRunnable>
#include <QTimer>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLTexture>
#include <QPainter>

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

            m_image = new QImage(100, 100, QImage::Format_RGBA8888);
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

    QQmlApplicationEngine engine;

    qmlRegisterType<TextureCanvas>("CustomComponents", 1, 0, "TextureCanvas");

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    return app.exec();
}

#include "main.moc"