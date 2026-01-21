import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Window {
    id: root
    visible: true
    width: 400
    height: 300
    title: "Project Settings"
    color: "#333333"

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 20

        // 解像度
        RowLayout {
            Label { text: "Resolution"; color: "white" }
            TextField {
                text: TimelineBridge ? TimelineBridge.projectWidth : "1920"
                onEditingFinished: if(TimelineBridge) TimelineBridge.projectWidth = parseInt(text)
            }
            Label { text: "x"; color: "white" }
            TextField {
                text: TimelineBridge ? TimelineBridge.projectHeight : "1080"
                onEditingFinished: if(TimelineBridge) TimelineBridge.projectHeight = parseInt(text)
            }
        }

        // FPS
        RowLayout {
            Label { text: "FPS"; color: "white" }
            ComboBox {
                model: [24, 30, 60]
                currentIndex: 2 // default 60
                onActivated: if(TimelineBridge) TimelineBridge.projectFps = model[index]
            }
        }
    }
}