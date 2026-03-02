import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 強さ/拡散/しきい値/拡散速度/光色/サイズ固定
    property real strength: Math.max(0, root.evalNumber("strength", 100) / 100)
    property real diffusion: Math.max(0, root.evalNumber("diffusion", 20))
    property real threshold: root.evalNumber("threshold", 0) / 100
    property real diffusionSpeed: Math.max(0, root.evalNumber("diffusionSpeed", 10))
    property color lightColor: root.evalColor("lightColor", "#ffffff")
    property bool fixedSize: root.evalParam("fixedSize", false)

    // パス1: しきい値で明輝部抽出 + 光色着色
    ShaderEffect {
        id: extractPass

        property variant source: root.sourceProxy
        property real threshold: root.threshold
        property color lightColor: root.lightColor

        anchors.fill: parent
        visible: false
        fragmentShader: "luminescence_extract.frag.qsb"
    }

    ShaderEffectSource {
        id: extractProxy

        sourceItem: extractPass
        hideSource: false
        visible: false
        live: true
    }

    // パス2/3: 水平・垂直ガウシアンブラー (blur.frag.qsb を再利用)
    ShaderEffect {
        id: hBlurPass

        property variant source: extractProxy
        property vector2d direction: Qt.vector2d(1, 0)
        property real radius: root.diffusionSpeed
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
        property real radius: root.diffusionSpeed
        property real gain: root.diffusion / 100
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

    // パス4: 加算合成 → fixedSize=false はそのまま出力
    ShaderEffect {
        id: compositePass

        property variant source: root.sourceProxy
        property variant glowSource: vBlurProxy
        property real strength: root.strength

        anchors.fill: parent
        visible: !root.fixedSize
        fragmentShader: "luminescence_composite.frag.qsb"
    }

    ShaderEffectSource {
        id: compositeProxy

        sourceItem: compositePass
        hideSource: false
        visible: false
        live: true
    }

    // fixedSize=true のとき元輪郭でマスク
    ShaderEffect {
        property variant source: compositeProxy
        property variant maskSource: root.sourceProxy

        anchors.fill: parent
        visible: root.fixedSize
        fragmentShader: "mask.frag.qsb"
    }

}
