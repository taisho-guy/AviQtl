import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 強さ/拡散/しきい値/ぼかし/形状/光色/光成分のみ
    property real strength: Math.max(0, root.evalNumber("strength", 100) / 100)
    property real diffusion: Math.max(0, root.evalNumber("diffusion", 20))
    property real threshold: root.evalNumber("threshold", 0) / 100
    property real blurSoft: Math.max(0, root.evalNumber("blur", 0))
    property color lightColor: root.evalColor("lightColor", "#ffffff")
    property bool glowOnly: root.evalParam("glowOnly", false)
    property real shape: {
        var s = root.evalParam("shape", "normal");
        if (s === "cross4")
            return 1;

        if (s === "cross4diag")
            return 2;

        if (s === "cross8")
            return 3;

        if (s === "lineH")
            return 4;

        if (s === "lineV")
            return 5;

        return 0;
    }

    // パス1: しきい値抽出 + 光色着色
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

    // パス2: 形状別ストリーク/ブラー
    ShaderEffect {
        id: streakPass

        property variant source: extractProxy
        property real diffusion: root.diffusion
        property real blurSoft: root.blurSoft
        property real shape: root.shape
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        visible: false
        fragmentShader: "glow_streak.frag.qsb"
    }

    ShaderEffectSource {
        id: streakProxy

        sourceItem: streakPass
        hideSource: false
        visible: false
        live: true
    }

    // パス3: 最終合成
    ShaderEffect {
        property variant source: root.sourceProxy
        property variant glowSource: streakProxy
        property real strength: root.strength
        property real glowOnly: root.glowOnly ? 1 : 0

        anchors.fill: parent
        fragmentShader: "glow_composite.frag.qsb"
    }

}
