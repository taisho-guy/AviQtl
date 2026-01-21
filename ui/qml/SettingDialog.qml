import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

Window {
    id: root
    width: 400
    height: 600
    title: "Object Settings"
    color: palette.window
    visible: true
    
    SystemPalette { id: palette; colorGroup: SystemPalette.Active }

    // ウィンドウ生成時に少し位置をずらす
    x: 500
    y: 200

    // データ同期用関数
    function updateUI() {
        if (!TimelineBridge) return;
        var data = TimelineBridge.selectedClipData;
        if (!data) return;

        // 値の更新 (ループ防止のため、UI操作中は更新しない等の制御が必要だが、
        // ここでは簡易的に全更新。Sliderのpressedプロパティ等でガード可能)
        
        if (!xSlider.pressed) xSlider.value = data["x"] !== undefined ? data["x"] : 0
        if (!ySlider.pressed) ySlider.value = data["y"] !== undefined ? data["y"] : 0
        if (!zSlider.pressed) zSlider.value = data["z"] !== undefined ? data["z"] : 0
        
        if (!scaleSlider.pressed) scaleSlider.value = data["scale"] !== undefined ? data["scale"] : 100
        if (!aspectSlider.pressed) aspectSlider.value = data["aspect"] !== undefined ? data["aspect"] : 0
        
        if (!rxSlider.pressed) rxSlider.value = data["rotationX"] !== undefined ? data["rotationX"] : 0
        if (!rySlider.pressed) rySlider.value = data["rotationY"] !== undefined ? data["rotationY"] : 0
        if (!rzSlider.pressed) rzSlider.value = data["rotationZ"] !== undefined ? data["rotationZ"] : 0
        
        if (!opacitySlider.pressed) opacitySlider.value = data["opacity"] !== undefined ? data["opacity"] : 1.0
        
        if (!colorField.activeFocus) colorField.text = data["color"] !== undefined ? data["color"] : "#ffffff"
        if (!textArea.activeFocus) textArea.text = data["text"] !== undefined ? data["text"] : ""
        if (!sizeSlider.pressed) sizeSlider.value = data["textSize"] !== undefined ? data["textSize"] : 64
    }

    Connections {
        target: TimelineBridge
        function onSelectedClipDataChanged() { updateUI() }
    }

    // 初期表示時にも更新
    Component.onCompleted: updateUI()

    // 共通スライダーコンポーネント
    component ParamSlider : RowLayout {
        property alias label: labelItem.text
        property alias value: sliderItem.value
        property alias from: sliderItem.from
        property alias to: sliderItem.to
        property string paramName: ""
        property bool isInt: false

        Label { 
            id: labelItem
            text: "Param"
            color: palette.text 
            Layout.preferredWidth: 80
        }
        Slider {
            id: sliderItem
            Layout.fillWidth: true
            onMoved: {
                if (TimelineBridge && paramName !== "") {
                    var v = isInt ? Math.round(value) : value
                    TimelineBridge.setClipProperty(paramName, v)
                }
            }
        }
        
        // AviUtl-like Draggable Number Field
        Item {
            Layout.preferredWidth: 60
            Layout.fillHeight: true
            
            // Input Field (Hidden normally, shown on click, or always shown but controlled by MouseArea)
            TextField {
                id: inputField
                anchors.fill: parent
                text: sliderItem.value.toFixed(isInt ? 0 : 2)
                color: palette.text
                background: Rectangle { color: "transparent" }
                horizontalAlignment: TextInput.AlignRight
                selectByMouse: false // Controlled by MouseArea
                readOnly: !activeFocus // Look read-only when not focused
                
                // Commit on Enter or focus loss
                onEditingFinished: {
                    var val = parseFloat(text);
                    if (!isNaN(val)) {
                        if (TimelineBridge && paramName !== "") {
                            TimelineBridge.setClipProperty(paramName, val);
                        }
                        sliderItem.value = val;
                    }
                    focus = false; // Remove focus
                }
            }

            MouseArea {
                id: dragArea
                anchors.fill: parent
                cursorShape: Qt.SizeHorCursor // Left-Right arrows
                
                property real startX: 0
                property real lastValue: 0
                property bool isDragging: false

                onPressed: (mouse) => {
                    startX = mouse.x
                    lastValue = sliderItem.value
                    isDragging = false
                    inputField.focus = false // Unfocus initially
                }

                onPositionChanged: (mouse) => {
                    // Treat as drag if moved more than threshold
                    if (Math.abs(mouse.x - startX) > 2) {
                        isDragging = true
                    }

                    if (isDragging) {
                        // Shift key for precision
                        var speed = 1.0;
                        if (mouse.modifiers & Qt.ShiftModifier) {
                            speed = 0.1;
                        }
                        
                        // Calculate delta
                        // Integer: 1px = 1 unit, Float: 1px = 0.1 unit
                        var step = isInt ? 1.0 : 0.1;
                        var delta = (mouse.x - startX) * step * speed;
                        
                        var newValue = lastValue + delta;
                        if (isInt) newValue = Math.round(newValue);
                        
                        // Update slider (which updates UI)
                        sliderItem.value = newValue;
                        
                        // Immediate update to backend
                        if (TimelineBridge && paramName !== "") {
                            TimelineBridge.setClipProperty(paramName, newValue);
                        }
                    }
                }

                onReleased: {
                    if (!isDragging) {
                        // Clicked without dragging -> Enter edit mode
                        inputField.forceActiveFocus();
                        inputField.selectAll();
                    }
                    isDragging = false;
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 5

        // ヘッダー
        Rectangle {
            Layout.fillWidth: true
            height: 30
            color: palette.mid
            Label {
                text: "Standard Drawing"
                anchors.centerIn: parent
                color: palette.text
                font.bold: true
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 8

                // --- 座標 ---
                Label { text: "Coordinates"; font.bold: true; color: palette.text }
                ParamSlider { id: xSlider; label: "X"; paramName: "x"; from: -1920; to: 1920; isInt: true }
                ParamSlider { id: ySlider; label: "Y"; paramName: "y"; from: -1080; to: 1080; isInt: true }
                ParamSlider { id: zSlider; label: "Z"; paramName: "z"; from: -2000; to: 2000; isInt: true }

                Rectangle { Layout.fillWidth: true; height: 1; color: palette.mid }

                // --- 拡大・回転 ---
                Label { text: "Transform"; font.bold: true; color: palette.text }
                ParamSlider { id: scaleSlider; label: "Scale (%)"; paramName: "scale"; from: 0; to: 500; }
                ParamSlider { id: aspectSlider; label: "Aspect"; paramName: "aspect"; from: -2.0; to: 2.0; }
                
                ParamSlider { id: rxSlider; label: "Rotate X"; paramName: "rotationX"; from: -360; to: 360; }
                ParamSlider { id: rySlider; label: "Rotate Y"; paramName: "rotationY"; from: -360; to: 360; }
                ParamSlider { id: rzSlider; label: "Rotate Z"; paramName: "rotationZ"; from: -360; to: 360; }

                Rectangle { Layout.fillWidth: true; height: 1; color: palette.mid }

                // --- マテリアル ---
                Label { text: "Material"; font.bold: true; color: palette.text }
                ParamSlider { id: opacitySlider; label: "Opacity"; paramName: "opacity"; from: 0; to: 1.0; }
                
                RowLayout {
                    Label { text: "Color"; color: palette.text; Layout.preferredWidth: 80 }
                    TextField {
                        id: colorField
                        Layout.fillWidth: true
                        placeholderText: "#RRGGBB"
                        onEditingFinished: {
                            if (TimelineBridge) TimelineBridge.setClipProperty("color", text)
                        }
                    }
                    Rectangle {
                        width: 20; height: 20
                        color: colorField.text
                        border.color: palette.text
                    }
                }

                // --- テキスト専用 ---
                Rectangle { Layout.fillWidth: true; height: 1; color: palette.mid }
                Label { text: "Text Settings"; font.bold: true; color: palette.text }
                
                TextArea {
                    id: textArea
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    color: palette.text
                    background: Rectangle { 
                        color: palette.base 
                        border.color: palette.mid
                    }
                    onEditingFinished: {
                        if(TimelineBridge) TimelineBridge.setClipProperty("text", text)
                    }
                }
                ParamSlider { id: sizeSlider; label: "Size"; paramName: "textSize"; from: 10; to: 200; isInt: true }

                Item { Layout.fillHeight: true } // Spacer
            }
        }
    }
}