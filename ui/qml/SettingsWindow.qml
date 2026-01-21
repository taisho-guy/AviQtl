import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "common" as Common

Common.RinaWindow {
    id: root
    visible: true
    width: 400
    height: 300
    title: "Project Settings"

    GridLayout {
        anchors.centerIn: parent
        columns: 2
        rowSpacing: 15
        columnSpacing: 15

        // 解像度
        Label { text: "Resolution:"; color: "white" }
        RowLayout {
            TextField {
                text: TimelineBridge ? TimelineBridge.projectWidth : "1920"
                onEditingFinished: if(TimelineBridge) TimelineBridge.projectWidth = parseInt(text)
                validator: IntValidator { bottom: 1; top: 8000 }
            }
            Label { text: "x"; color: "white" }
            TextField {
                text: TimelineBridge ? TimelineBridge.projectHeight : "1080"
                onEditingFinished: if(TimelineBridge) TimelineBridge.projectHeight = parseInt(text)
                validator: IntValidator { bottom: 1; top: 8000 }
            }
        }

        // FPS
        Label { text: "FPS:"; color: "white" }
        ComboBox {
            id: fpsCombo
            model: [24, 30, 60]
            
            // 修正: 初期化時にモデルから探して設定
            Component.onCompleted: {
                var currentFps = TimelineBridge ? TimelineBridge.projectFps : 60
                var idx = fpsCombo.find(currentFps)
                fpsCombo.currentIndex = idx
            }

            // ヘルパー関数: モデル配列から値を探す
            function find(val) {
                // modelはJS配列なので直接indexOfが使える
                var idx = model.indexOf(val)
                return idx >= 0 ? idx : 2 // Default to 60 (index 2)
            }

            onActivated: (index) => {
                if(TimelineBridge) TimelineBridge.projectFps = model[index]
            }
        }
        
        // 総フレーム数
        Label { text: "Duration (Frames):"; color: "white" }
        TextField {
            text: TimelineBridge ? TimelineBridge.totalFrames : "3000"
            onEditingFinished: if(TimelineBridge) TimelineBridge.totalFrames = parseInt(text)
            validator: IntValidator { bottom: 1 }
        }
    }
}