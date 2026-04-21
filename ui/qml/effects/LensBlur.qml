import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 範囲 0-500、光の強さ 0-100、サイズ固定
    property real range: Math.max(0, root.evalNumber("radius", 10))
    property real lightPower: Math.max(0, root.evalNumber("brightness", 0))
    property bool fixedSize: root.evalParam("fixedSize", false)

    // 描画範囲を radius 分だけ全方向に拡張
    expansion: ({
        "top": range,
        "right": range,
        "bottom": range,
        "left": range
    })

    ShaderEffect {
        id: blurPass

        property variant source: root.sourceProxy
        property real radius: root.range
        property real brightness: root.lightPower
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        visible: !root.fixedSize
        fragmentShader: "lensblur.frag.qsb"
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
