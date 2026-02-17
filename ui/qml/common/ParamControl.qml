import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// AviUtl風の中間点対応パラメータコントロール
RowLayout {
    id: root

    property string paramName: "パラメータ"
    property real minValue: 0
    property real maxValue: 100
    property real startValue: 0
    property real endValue: 0
    property int decimals: 0
    property string interpolationType: "linear"
    property bool enabled: true
    property bool isRangeMode: Math.abs(startValue - endValue) > 0.001

    signal startValueModified(real value)
    signal endValueModified(real value)
    signal paramButtonClicked()

    spacing: 8

    SystemPalette {
        id: palette

        colorGroup: SystemPalette.Active
    }

    // 左側スライダー（開始値）
    Slider {
        id: leftSlider

        Layout.fillWidth: true
        Layout.preferredWidth: 120
        from: root.minValue
        to: root.maxValue
        enabled: root.enabled
        Component.onCompleted: value = root.startValue
        onMoved: {
            root.startValueModified(decimals === 0 ? Math.round(value) : value);
        }

        Connections {
            function onStartValueChanged() {
                if (!leftSlider.pressed)
                    leftSlider.value = root.startValue;

            }

            target: root
        }

    }

    // 左側数値ボックス
    TextField {
        id: leftValueField

        Layout.preferredWidth: 70
        // 明示的なプロパティバインディング
        text: (root.decimals === 0) ? root.startValue.toFixed(0) : root.startValue.toFixed(root.decimals)
        horizontalAlignment: TextInput.AlignHCenter
        selectByMouse: true
        enabled: root.enabled
        onAccepted: {
            // Enter確定時
            var newVal = parseFloat(text);
            if (!isNaN(newVal))
                root.startValueModified(newVal);

        }
        onActiveFocusChanged: {
            if (!activeFocus)
                onAccepted();

        } // フォーカス外れ時も同期

        validator: DoubleValidator {
            bottom: root.minValue
            top: root.maxValue
            decimals: root.decimals
        }

    }

    // 中央ボタン
    Button {
        id: paramButton

        Layout.preferredWidth: 100
        text: root.paramName
        enabled: root.enabled
        onClicked: root.paramButtonClicked()
    }

    // 右側数値ボックス
    TextField {
        id: rightValueField

        Layout.preferredWidth: 70
        text: (root.decimals === 0) ? root.endValue.toFixed(0) : root.endValue.toFixed(root.decimals)
        horizontalAlignment: TextInput.AlignHCenter
        selectByMouse: true
        enabled: root.enabled
        onAccepted: {
            var newVal = parseFloat(text);
            if (!isNaN(newVal))
                root.endValueModified(newVal);

        }
        onActiveFocusChanged: {
            if (!activeFocus)
                onAccepted();

        }

        validator: DoubleValidator {
            bottom: root.minValue
            top: root.maxValue
            decimals: root.decimals
        }

    }

    // 右側スライダー（終了値）
    Slider {
        id: rightSlider

        Layout.fillWidth: true
        Layout.preferredWidth: 120
        from: root.minValue
        to: root.maxValue
        enabled: root.enabled
        Component.onCompleted: value = root.endValue
        onMoved: {
            root.endValueModified(decimals === 0 ? Math.round(value) : value);
        }

        Connections {
            function onEndValueChanged() {
                if (!rightSlider.pressed)
                    rightSlider.value = root.endValue;

            }

            target: root
        }

    }

}
