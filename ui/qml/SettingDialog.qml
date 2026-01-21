import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

Window {
    id: root
    width: 350
    height: 400
    title: "Standard Draw [3D Box]"
    color: "#333"
    visible: true
    
    // ウィンドウ生成時に少し位置をずらす
    x: 500
    y: 200

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 5

        // ヘッダー
        Rectangle {
            Layout.fillWidth: true
            height: 30
            color: "#444"
            Label {
                text: "Standard Draw"
                anchors.centerIn: parent
                color: "white"
                font.bold: true
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#555" }
        Label { text: "Text Object"; font.bold: true; color: "#aaa" }

        // テキスト入力
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            TextArea {
                id: textArea
                text: (TimelineBridge && TimelineBridge.selectedClipData.text !== undefined) ? TimelineBridge.selectedClipData.text : ""
                color: "white"
                background: Rectangle { color: "#222" }
                // 入力ループ防止のため、activeFocusがある時のみ更新
                onTextChanged: {
                    if (TimelineBridge && activeFocus) TimelineBridge.setClipProperty("text", text)
                }
            }
        }

        // X座標
        RowLayout {
            Label { text: "X"; color: "white"; Layout.preferredWidth: 30 }
            Slider {
                id: xSlider
                Layout.fillWidth: true
                from: -500; to: 500
                value: (TimelineBridge && TimelineBridge.selectedClipData.x !== undefined) ? TimelineBridge.selectedClipData.x : 0
                onMoved: if (TimelineBridge) TimelineBridge.setClipProperty("x", Math.round(value))
            }
            
            Button {
                text: "+"
                Layout.preferredWidth: 30
                onClicked: {
                    if (TimelineBridge)
                        TimelineBridge.addKeyframe(TimelineBridge.currentFrame, Math.round(xSlider.value)) 
                }
            }

            TextField {
                text: (TimelineBridge && TimelineBridge.selectedClipData.x !== undefined) ? TimelineBridge.selectedClipData.x.toFixed(0) : "0"
                Layout.preferredWidth: 60
                onEditingFinished: if (TimelineBridge) TimelineBridge.setClipProperty("x", parseInt(text))
            }
        }
        
        // Y座標
        RowLayout {
            Label { text: "Y"; color: "white"; Layout.preferredWidth: 30 }
            Slider {
                id: ySlider
                Layout.fillWidth: true
                from: -500; to: 500
                value: (TimelineBridge && TimelineBridge.selectedClipData.y !== undefined) ? TimelineBridge.selectedClipData.y : 0
                onMoved: if (TimelineBridge) TimelineBridge.setClipProperty("y", Math.round(value))
            }
            TextField {
                text: (TimelineBridge && TimelineBridge.selectedClipData.y !== undefined) ? TimelineBridge.selectedClipData.y.toFixed(0) : "0"
                Layout.preferredWidth: 60
                onEditingFinished: if (TimelineBridge) TimelineBridge.setClipProperty("y", parseInt(text))
            }
        }

        // Width / Height Controls
        RowLayout {
            Label { text: "W"; color: "white"; Layout.preferredWidth: 30 }
            Slider {
                id: wSlider
                Layout.fillWidth: true; from: 1; to: 1920
                value: (TimelineBridge && TimelineBridge.selectedClipData.width !== undefined) ? TimelineBridge.selectedClipData.width : 100
                onMoved: if(TimelineBridge) TimelineBridge.setClipProperty("width", Math.round(value))
            }
            TextField {
                text: (TimelineBridge && TimelineBridge.selectedClipData.width !== undefined) ? TimelineBridge.selectedClipData.width.toString() : "100"
                Layout.preferredWidth: 60
                onEditingFinished: if(TimelineBridge) TimelineBridge.setClipProperty("width", parseInt(text))
            }
        }
        RowLayout {
            Label { text: "H"; color: "white"; Layout.preferredWidth: 30 }
            Slider {
                id: hSlider
                Layout.fillWidth: true; from: 1; to: 1080
                value: (TimelineBridge && TimelineBridge.selectedClipData.height !== undefined) ? TimelineBridge.selectedClipData.height : 100
                onMoved: if(TimelineBridge) TimelineBridge.setClipProperty("height", Math.round(value))
            }
            TextField {
                text: (TimelineBridge && TimelineBridge.selectedClipData.height !== undefined) ? TimelineBridge.selectedClipData.height.toString() : "100"
                Layout.preferredWidth: 60
                onEditingFinished: if(TimelineBridge) TimelineBridge.setClipProperty("height", parseInt(text))
            }
        }

        // テキストサイズ
        RowLayout {
            Label { text: "Size"; color: "white"; Layout.preferredWidth: 30 }
            Slider {
                id: sizeSlider
                Layout.fillWidth: true
                from: 10; to: 200
                value: (TimelineBridge && TimelineBridge.selectedClipData.textSize !== undefined) ? TimelineBridge.selectedClipData.textSize : 64
                onMoved: if (TimelineBridge) TimelineBridge.setClipProperty("textSize", Math.round(value))
            }
            Label {
                text: sizeSlider.value.toFixed(0)
                color: "white"
                Layout.preferredWidth: 40
                horizontalAlignment: Text.AlignRight
            }
        }

        Item { Layout.fillHeight: true } // スペーサー

        Connections {
            target: TimelineBridge
            function onSelectedClipDataChanged() {
                if (!TimelineBridge) return;
                var data = TimelineBridge.selectedClipData;
                if (data.x !== undefined) xSlider.value = data.x;
                if (data.y !== undefined) ySlider.value = data.y;
                if (data.width !== undefined) wSlider.value = data.width;
                if (data.height !== undefined) hSlider.value = data.height;
                if (data.textSize !== undefined) sizeSlider.value = data.textSize;
            }
        }
    }
}