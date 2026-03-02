import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 強さ 0-100、拡散 0-500
    property real strength: Math.max(0, Math.min(1, root.evalNumber("strength", 100) / 100))
    property real diffusion: Math.max(0, root.evalNumber("diffusion", 20))

    ShaderEffect {
        id: hBlurPass

        property variant source: root.sourceProxy
        property vector2d direction: Qt.vector2d(1, 0)
        property real radius: root.diffusion
        property real gain: 0
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        visible: false
        fragmentShader: "blur.frag.qsb"
    }

    ShaderEffectSource {
        id: hBlurProxy

        sourceItem: hBlurPass
        hideSource: false
        visible: false
        live: true
    }

    ShaderEffect {
        id: vBlurPass

        property variant source: hBlurProxy
        property vector2d direction: Qt.vector2d(0, 1)
        property real radius: root.diffusion
        property real gain: 0
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        visible: false
        fragmentShader: "blur.frag.qsb"
    }

    ShaderEffectSource {
        id: vBlurProxy

        sourceItem: vBlurPass
        hideSource: false
        visible: false
        live: true
    }

    ShaderEffect {
        property variant source: root.sourceProxy
        property variant blurSource: vBlurProxy
        property real strength: root.strength

        anchors.fill: parent
        fragmentShader: "diffusedlight_composite.frag.qsb"
    }

}
