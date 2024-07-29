import QtQuick 2.0
import QtQuick.Controls 1.0
import CustomComponents 1.0

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

        TextureCanvas {
            width: 400
            height: 400
        }
    }
}