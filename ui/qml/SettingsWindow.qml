import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "common" as Common
import "common" // ParamControl用

Common.RinaWindow {
    id: root
    visible: true
    width: 450
    height: 250
    minimumWidth: 400
    minimumHeight: 200
    title: "Project Settings"

    GridLayout {
        anchors.fill: parent
        anchors.margins: 20
        columns: 2
        rowSpacing: 20
        columnSpacing: 20

        // 解像度
        Label { text: "Resolution:"; color: palette.text }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                text: TimelineBridge ? TimelineBridge.projectWidth : "1920"
                onEditingFinished: if(TimelineBridge) TimelineBridge.projectWidth = parseInt(text)
                validator: IntValidator { bottom: 1; top: 8000 }
                Layout.fillWidth: true
                horizontalAlignment: TextInput.AlignHCenter
            }
            Label { text: "x"; color: palette.text }
            TextField {
                text: TimelineBridge ? TimelineBridge.projectHeight : "1080"
                onEditingFinished: if(TimelineBridge) TimelineBridge.projectHeight = parseInt(text)
                validator: IntValidator { bottom: 1; top: 8000 }
                Layout.fillWidth: true
                horizontalAlignment: TextInput.AlignHCenter
            }
        }

        // FPS
        Label { text: "FPS:"; color: palette.text }
        ComboBox {
            id: fpsCombo
            Layout.fillWidth: true
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
        Label { text: "Duration (Frames):"; color: palette.text }
        TextField {
            Layout.fillWidth: true
            text: TimelineBridge ? TimelineBridge.totalFrames : "3000"
            onEditingFinished: if(TimelineBridge) TimelineBridge.totalFrames = parseInt(text)
            validator: IntValidator { bottom: 1 }
        }

        // 区切り線
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#444"
            Layout.columnSpan: 2
        }

        // オブジェクトタイプに応じた設定
        Loader {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: {
                if (!TimelineBridge) return null;
                if (TimelineBridge.activeObjectType === "text") return textSettings;
                return shapeSettings;
            }
        }
    }
    
    // テキストオブジェクト用設定
    Component {
        id: textSettings
        ScrollView {
            clip: true
            
            ColumnLayout {
                width: parent.width
                spacing: 10
                
                Label { 
                    text: "テキスト設定"
                    color: palette.text
                    font.bold: true
                }
                
                // テキスト内容（中間点非対応）
                RowLayout {
                    Layout.fillWidth: true
                    Label { 
                        text: "テキスト:"
                        color: palette.text
                        Layout.preferredWidth: 80
                    }
                    TextField {
                        Layout.fillWidth: true
                        text: TimelineBridge.selectedClipData.text !== undefined ? TimelineBridge.selectedClipData.text : ""
                        onEditingFinished: TimelineBridge.setClipProperty("text", text)
                    }
                }
                
                // X座標（中間点対応）
                ParamControl {
                    Layout.fillWidth: true
                    paramName: "X"
                    minValue: -1920
                    maxValue: 3840
                    decimals: 0
                    startValue: TimelineBridge.selectedClipData.x !== undefined ? TimelineBridge.selectedClipData.x : 0
                    endValue: TimelineBridge.selectedClipData.x !== undefined ? TimelineBridge.selectedClipData.x : 0
                    onStartValueModified: (value) => TimelineBridge.setClipProperty("x", value)
                    onEndValueModified: (value) => TimelineBridge.setClipProperty("x", value)
                }
                
                // Y座標（中間点対応）
                ParamControl {
                    Layout.fillWidth: true
                    paramName: "Y"
                    minValue: -1080
                    maxValue: 2160
                    decimals: 0
                    startValue: TimelineBridge.selectedClipData.y !== undefined ? TimelineBridge.selectedClipData.y : 0
                    endValue: TimelineBridge.selectedClipData.y !== undefined ? TimelineBridge.selectedClipData.y : 0
                    onStartValueModified: (value) => TimelineBridge.setClipProperty("y", value)
                    onEndValueModified: (value) => TimelineBridge.setClipProperty("y", value)
                }
                
                // 拡大率（中間点対応）
                ParamControl {
                    Layout.fillWidth: true
                    paramName: "拡大率"
                    minValue: 0
                    maxValue: 800
                    decimals: 1
                    startValue: TimelineBridge.selectedClipData.scale !== undefined ? TimelineBridge.selectedClipData.scale : 100
                    endValue: TimelineBridge.selectedClipData.scale !== undefined ? TimelineBridge.selectedClipData.scale : 100
                    onStartValueModified: (value) => TimelineBridge.setClipProperty("scale", value)
                    onEndValueModified: (value) => TimelineBridge.setClipProperty("scale", value)
                }
                
                Item { Layout.fillHeight: true }  // Spacer
            }
        }
    }
    
    // 図形オブジェクト用設定
    Component {
        id: shapeSettings
        ColumnLayout {
            Label { 
                text: "図形設定（未実装）"
                color: palette.text
            }
            Item { Layout.fillHeight: true }
        }
    }

}