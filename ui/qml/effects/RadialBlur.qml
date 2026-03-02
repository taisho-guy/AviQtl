import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 範囲 0-2000、X/Y -2000~2000、サイズ固定
    property real range: Math.max(0, root.evalNumber("range", 50))
    property real centerX: root.evalNumber("x", 0)
    property real centerY: root.evalNumber("y", 0)
    property bool fixedSize: root.evalParam("fixedSize", false)
    // samples: 品質 (内部計算: range が大きいほど多く)
    readonly property real samples: Math.max(1, Math.min(64, Math.ceil(range / 8) + 4))
    readonly property real strength: range

    ShaderEffect {
        id: blurPass

        property variant source: root.sourceProxy
        property real samples: root.samples
        property real strength: root.strength
        property real centerX: root.centerX
        property real centerY: root.centerY
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        visible: !root.fixedSize
        fragmentShader: "radialblur.frag.qsb"
    }

    ShaderEffectSource {
        id: blurProxy

        sourceItem: blurPass
        hideSource: false
        visible: false
        live: true
    }

    ShaderEffect {
        property variant source: blurProxy
        property variant maskSource: root.sourceProxy

        anchors.fill: parent
        visible: root.fixedSize
        fragmentShader: "mask.frag.qsb"
    }

}
