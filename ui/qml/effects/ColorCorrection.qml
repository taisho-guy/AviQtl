import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // MultiEffectの描画を無効化
    maskEnabled: true
    maskSource: emptyMask

    Item {
        id: emptyMask

        visible: false
    }

    ShaderEffect {
        property variant source: root.sourceProxy
        // パラメータを -1.0 ~ 1.0 (または適切な範囲) に正規化して渡す
        property real brightness: (root.evalNumber("brightness", 100) - 100) / 100
        property real contrast: (root.evalNumber("contrast", 100) - 100) / 100
        property real hue: root.evalNumber("hue", 0) / 360
        property real saturation: (root.evalNumber("saturation", 100) - 100) / 100
        property real lightness: (root.evalNumber("lightness", 100) - 100) / 100

        anchors.fill: parent
        fragmentShader: "color_correction.frag.qsb"
    }

}
