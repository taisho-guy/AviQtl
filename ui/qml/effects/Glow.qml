import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real strength: Math.max(0, root.evalNumber("strength", 100)) / 100
    property real diffusion: Math.max(0, root.evalNumber("diffusion", 10))
    property real threshold: Math.max(0, Math.min(100, root.evalNumber("threshold", 10))) / 100
    property real speed: root.evalNumber("speed", 0)
    property bool useOriginalColor: root.evalParam("useOriginalColor", true)
    property color glowColor: root.evalColor("color", "#ffffff")
    property bool fixedSize: root.evalParam("fixedSize", false)
    // 拡散速度（ゆらぎ）の計算
    property real fluctuation: {
        if (speed <= 0)
            return 1;

        // フレーム数ベースの簡易ノイズ (sin/cos合成)
        var t = root.frame * speed * 0.05;
        return 1 + (Math.sin(t) * 0.2 + Math.cos(t * 2.3) * 0.1);
    }
    property real effectiveDiffusion: Math.max(0, diffusion * fluctuation)

    // 1. 発光層 (背面)
    MultiEffect {
        anchors.fill: parent
        source: root.sourceProxy
        blurEnabled: true
        blurMax: 64
        blur: Math.min(1, root.effectiveDiffusion / 64)
        colorization: root.useOriginalColor ? 0 : 1
        colorizationColor: root.glowColor
        opacity: Math.min(1, root.strength / 100)
    }

    // 2. 元画像 (前面)
    ShaderEffect {
        property variant source: root.sourceProxy

        anchors.fill: parent
    }

}
