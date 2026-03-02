import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 範囲 0-500、角度 0-360、サイズ固定
    property real range: Math.max(0, root.evalNumber("range", 20))
    property real angle: root.evalNumber("angle", 0)
    property bool fixedSize: root.evalParam("fixedSize", false)

    ShaderEffect {
        id: blurPass

        property variant source: root.sourceProxy
        property real angle: root.angle
        property real len: root.range
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        visible: !root.fixedSize
        fragmentShader: "directionalblur.frag.qsb"
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
