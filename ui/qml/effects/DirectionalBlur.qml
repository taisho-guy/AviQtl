import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 範囲 0-500、角度 0-360、サイズ固定
    property real range: Math.max(0, root.evalNumber("length", 10))
    property real angle: root.evalNumber("angle", 0)
    property bool fixedSize: root.evalParam("fixedSize", false)
    // angle 方向に range 分だけ拡張（水平・垂直分解して非対称余白を計算）
    readonly property real _rad: angle * Math.PI / 180

    expansion: ({
        "top": Math.abs(Math.sin(_rad)) * range,
        "right": Math.abs(Math.cos(_rad)) * range,
        "bottom": Math.abs(Math.sin(_rad)) * range,
        "left": Math.abs(Math.cos(_rad)) * range
    })

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
