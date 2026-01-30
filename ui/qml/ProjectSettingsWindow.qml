import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "common" as Common

Common.RinaWindow {
    id: root
    width: 450
    height: 250
    title: "プロジェクト設定"
    
    // ウィンドウが表示されたときに現在の値をUIに反映
    onVisibleChanged: {
        if (visible && TimelineBridge && TimelineBridge.project) {
            widthField.text = TimelineBridge.project.width
            heightField.text = TimelineBridge.project.height
            fpsField.text = TimelineBridge.project.fps
            framesField.text = TimelineBridge.project.totalFrames
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 15
        
        GridLayout {
            columns: 2
            rowSpacing: 10
            columnSpacing: 10

            Label { text: "幅:" }
            TextField { 
                id: widthField
                validator: IntValidator{ bottom: 1; top: 8000 }
                selectByMouse: true
            }

            Label { text: "高さ:" }
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
            
            Label { text: "総フレーム数:" }
            TextField { 
                id: framesField
                validator: IntValidator{ bottom: 1; top: 1000000 }
                selectByMouse: true
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Button {
                text: "適用"
                highlighted: true
                onClicked: {
                    if (TimelineBridge && TimelineBridge.project) {
                        TimelineBridge.project.width = parseInt(widthField.text)
                        TimelineBridge.project.height = parseInt(heightField.text)
                        TimelineBridge.project.fps = parseFloat(fpsField.text)
                        TimelineBridge.project.totalFrames = parseInt(framesField.text)
                        root.hide()
                    }
                }
            }
            Button {
                text: "キャンセル"
                onClicked: root.hide()
            }
        }
    }
}