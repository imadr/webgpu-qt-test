#include <QApplication>
#include <QGuiApplication>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QTimer>

class TextureProvider : public QObject, public QQuickImageProvider {
    Q_OBJECT
   public:
    TextureProvider() : QQuickImageProvider(QQuickImageProvider::Image) {
    }

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override {
        if (size) *size = m_image.size();
        return m_image;
    }

    void updateTexture(const QImage &image) {
        m_image = image;
        emit textureUpdated();
    }

    void updateTextureFromBytes(const QByteArray &bytes, int width, int height) {
        QImage image(reinterpret_cast<const uchar *>(bytes.constData()), width, height,
                     QImage::Format_RGB32);
        if (image.isNull()) {
            qWarning() << "Failed to create QImage from bytes";
            return;
        }

        m_image = image;
        emit textureUpdated();
    }

   signals:
    void textureUpdated();

   private:
    QImage m_image;
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    TextureProvider textureProvider;

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("textureProvider", &textureProvider);
    engine.addImageProvider(QLatin1String("canvastexture"), &textureProvider);

    /*QImage exampleImage(700, 700, QImage::Format_RGB32);
    exampleImage.fill(Qt::red);
    textureProvider.updateTexture(exampleImage);*/

    QByteArray byteArray;
    byteArray.resize(700 * 700 * 4);

    for (int y = 0; y < 700; ++y) {
        for (int x = 0; x < 700; ++x) {
            int index = (y * 700 + x) * 4;
            byteArray[index + 0] = 0;
            byteArray[index + 1] = 255;
            byteArray[index + 2] = 255;
            byteArray[index + 3] = 255;
        }
    }
    textureProvider.updateTextureFromBytes(byteArray, 700, 700);

    QTimer::singleShot(1000, [&textureProvider]() {
        QImage exampleImage2(700, 700, QImage::Format_RGB32);
        exampleImage2.fill(Qt::green);
        textureProvider.updateTexture(exampleImage2);
    });

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    return app.exec();
}

#include "main.moc"