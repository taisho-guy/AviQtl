import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// AviUtl風の中間点対応パラメータコントロール
RowLayout {
    id: root
    spacing: 8
    
    property string paramName: "Parameter"
    property real minValue: 0
    property real maxValue: 100
    property real startValue: 0
    property real endValue: 0
    property int decimals: 0
    property string interpolationType: "linear"
    property bool enabled: true
    
    signal startValueModified(real value)
    signal endValueModified(real value)
    signal paramButtonClicked()
    
    property bool isRangeMode: Math.abs(startValue - endValue) > 0.001
    
    SystemPalette { id: palette; colorGroup: SystemPalette.Active }
    
    // 左側スライダー（開始値）
    Slider {
        id: leftSlider
        Layout.fillWidth: true
        Layout.preferredWidth: 120
        from: root.minValue
        to: root.maxValue
        value: root.startValue
        enabled: root.enabled
        
        onMoved: {
            var newVal = decimals === 0 ? Math.round(value) : value
            root.startValueModified(newVal)
        }
        
        background: Rectangle {
            x: leftSlider.leftPadding
            y: leftSlider.topPadding + leftSlider.availableHeight / 2 - height / 2
            width: leftSlider.availableWidth
            height: 4
            radius: 2
            color: "#3a3a3a"
            
            Rectangle {
                width: leftSlider.visualPosition * parent.width
                height: parent.height
                color: root.isRangeMode ? "#4a9eff" : "#888888"
                radius: 2
            }
        }
    }
    
    // 左側数値ボックス
    TextField {
        id: leftValueField
        Layout.preferredWidth: 70
        text: root.decimals === 0 ? root.startValue.toFixed(0) : root.startValue.toFixed(root.decimals)
        horizontalAlignment: TextInput.AlignHCenter
        selectByMouse: true
        enabled: root.enabled
        validator: DoubleValidator { 
            bottom: root.minValue
            top: root.maxValue
            decimals: root.decimals
        }
        
        onEditingFinished: {
            var newVal = parseFloat(text)
            if (!isNaN(newVal)) {
                root.startValueModified(newVal)
            }
        }
    }
    
    // 中央ボタン
    Button {
        id: paramButton
        Layout.preferredWidth: 100
        text: root.paramName
        enabled: root.enabled
        
        contentItem: Column {
            spacing: 2
            Text {
                text: paramButton.text
                color: paramButton.enabled ? palette.buttonText : palette.mid
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
                font.pointSize: 10
            }
            Text {
                text: getInterpolationLabel()
                color: paramButton.enabled ? "#4a9eff" : palette.mid
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
                font.pointSize: 8
                visible: root.isRangeMode
            }
        }
        
        onClicked: root.paramButtonClicked()
        
        function getInterpolationLabel() {
            switch(root.interpolationType) {
                case "linear": return "━━"
                case "ease_in": return "╱━"
                case "ease_out": return "━╲"
                case "ease_in_out": return "╱╲"
                default: return "━━"
            }
        }
    }
    
    // 右側数値ボックス
    TextField {
        id: rightValueField
        Layout.preferredWidth: 70
        text: root.decimals === 0 ? root.endValue.toFixed(0) : root.endValue.toFixed(root.decimals)
        horizontalAlignment: TextInput.AlignHCenter
        selectByMouse: true
        enabled: root.enabled
        validator: DoubleValidator { 
            bottom: root.minValue
            top: root.maxValue
            decimals: root.decimals
        }
        
        onEditingFinished: {
            var newVal = parseFloat(text)
            if (!isNaN(newVal)) {
                root.endValueModified(newVal)
            }
        }
    }
    
    // 右側スライダー（終了値）
    Slider {
        id: rightSlider
        Layout.fillWidth: true
        Layout.preferredWidth: 120
        from: root.minValue
        to: root.maxValue
        value: root.endValue
        enabled: root.enabled
        
        onMoved: {
            var newVal = decimals === 0 ? Math.round(value) : value
            root.endValueModified(newVal)
        }
        
        background: Rectangle {
            x: rightSlider.leftPadding
            y: rightSlider.topPadding + rightSlider.availableHeight / 2 - height / 2
            width: rightSlider.availableWidth
            height: 4
            radius: 2
            color: "#3a3a3a"
            
            Rectangle {
                width: rightSlider.visualPosition * parent.width
                height: parent.height
                color: root.isRangeMode ? "#4a9eff" : "#888888"
                radius: 2
            }
        }
    }
}