import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "common" as Common

Common.RinaWindow {
    id: root
    width: 450
    height: 250
    title: "Project Settings"
    
    // ウィンドウが表示されたときに現在の値をUIに反映
    onVisibleChanged: {
        if (visible && TimelineBridge) {
            widthField.text = TimelineBridge.projectWidth
            heightField.text = TimelineBridge.projectHeight
            fpsField.text = TimelineBridge.projectFps
            framesField.text = TimelineBridge.totalFrames
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 15
        
        GridLayout {
            columns: 2
            rowSpacing: 10
            columnSpacing: 10

            Label { text: "Width:" }
            TextField { 
                id: widthField
                validator: IntValidator{ bottom: 1; top: 8000 }
                selectByMouse: true
            }

            Label { text: "Height:" }
            TextField { 
                id: heightField
                validator: IntValidator{ bottom: 1; top: 8000 }
                selectByMouse: true
            }

            Label { text: "FPS:" }
            TextField { 
                id: fpsField
                validator: DoubleValidator{ bottom: 1.0; top: 240.0 }
                selectByMouse: true
            }
            
            Label { text: "Total Frames:" }
            TextField { 
                id: framesField
                validator: IntValidator{ bottom: 1; top: 1000000 }
                selectByMouse: true
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Button {
                text: "Apply"
                highlighted: true
                onClicked: {
                    if (TimelineBridge) {
                        TimelineBridge.setProjectWidth(parseInt(widthField.text))
                        TimelineBridge.setProjectHeight(parseInt(heightField.text))
                        TimelineBridge.setProjectFps(parseFloat(fpsField.text))
                        TimelineBridge.setTotalFrames(parseInt(framesField.text))
                        root.hide()
                    }
                }
            }
            Button {
                text: "Cancel"
                onClicked: root.hide()
            }
        }
    }
}