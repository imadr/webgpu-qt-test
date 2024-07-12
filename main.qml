import QtQuick 2.0
import QtQuick.Controls 1.0

ApplicationWindow {
    visible: true
    width: 800
    height: 800
    title: qsTr("webgpu-qt-test")

    Column {
        anchors.centerIn: parent

        Button {
            text: qsTr("Click Me")
            onClicked: {
                console.log("Button clicked!")
            }
        }

        Image {
            id: dynamicImage
            width: 700
            height: 700
            source: "image://canvastexture/texture"
            sourceSize: Qt.size(700, 700)

            Connections {
                target: textureProvider
                function onTextureUpdated() {
                    dynamicImage.source = "";
                    dynamicImage.source = "image://canvastexture/texture"
                    dynamicImage.sourceSize = Qt.size(640, 480)
                }
            }
        }
    }
}