import QtQuick 2.0
import QtQuick.Controls 1.0

ApplicationWindow
{
    visible: true
    width: 640
    height: 480
    title: qsTr("webgpu-qt-test")

    Button {
        text: qsTr("Click Me")
        anchors.centerIn: parent
        onClicked: {
            console.log("Button clicked!")
        }
    }
}