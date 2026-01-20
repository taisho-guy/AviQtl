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
                text: TimelineBridge ? TimelineBridge.textString : ""
                color: "white"
                background: Rectangle { color: "#222" }
                // 入力するたびに即時反映
                onTextChanged: {
                    if (TimelineBridge) TimelineBridge.textString = text
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
                // 初期値
                value: TimelineBridge ? TimelineBridge.objectX : 0
                // バインディング: スライダー -> C++
                onMoved: if (TimelineBridge) TimelineBridge.objectX = value
                
                // バインディング: C++ -> スライダー (外部要因で変わった場合)
                Connections {
                    target: TimelineBridge
                    function onObjectXChanged() {
                        if (TimelineBridge) xSlider.value = TimelineBridge.objectX
                    }
                }
            }
            
            Button {
                text: "+"
                Layout.preferredWidth: 30
                onClicked: {
                    if (TimelineBridge)
                        TimelineBridge.addKeyframe(Math.round(TimelineBridge.currentTime), xSlider.value)
                }
            }

            TextField {
                text: TimelineBridge ? Math.round(TimelineBridge.objectX) : "0"
                Layout.preferredWidth: 60
                onEditingFinished: if (TimelineBridge) TimelineBridge.objectX = parseFloat(text)
            }
        }
        
        // Y座標
        RowLayout {
            Label { text: "Y"; color: "white"; Layout.preferredWidth: 30 }
            Slider {
                id: ySlider
                Layout.fillWidth: true
                from: -500; to: 500
                value: TimelineBridge ? TimelineBridge.objectY : 0
                onMoved: if (TimelineBridge) TimelineBridge.objectY = value
                
                Connections {
                    target: TimelineBridge
                    function onObjectYChanged() {
                        if (TimelineBridge) ySlider.value = TimelineBridge.objectY
                    }
                }
            }
            TextField {
                text: TimelineBridge ? Math.round(TimelineBridge.objectY) : "0"
                Layout.preferredWidth: 60
                onEditingFinished: if (TimelineBridge) TimelineBridge.objectY = parseFloat(text)
            }
        }

        // テキストサイズ
        RowLayout {
            Label { text: "Size"; color: "white"; Layout.preferredWidth: 30 }
            Slider {
                id: sizeSlider
                Layout.fillWidth: true
                from: 10; to: 200
                value: TimelineBridge ? TimelineBridge.textSize : 64
                onMoved: if (TimelineBridge) TimelineBridge.textSize = value
            }
            Label {
                text: sizeSlider.value.toFixed(0)
                color: "white"
                Layout.preferredWidth: 40
                horizontalAlignment: Text.AlignRight
            }
        }

        Item { Layout.fillHeight: true } // スペーサー
    }
}