import Qt5Compat.GraphicalEffects
import QtQuick
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

    // 1. しきい値処理 (ThresholdMask)
    // 元画像をマスクとして使い、輝度がしきい値以下の部分を切り落とす
    ThresholdMask {
        id: thresholded

        anchors.fill: parent
        source: root.source
        maskSource: root.source
        threshold: root.threshold
        spread: 0.1
        visible: false
    }

    // 2. 着色 (ColorOverlay) - オプション
    ColorOverlay {
        id: colored

        anchors.fill: parent
        source: thresholded
        color: root.glowColor
        visible: false
    }

    // 3. ぼかし (FastBlur)
    FastBlur {
        id: blurred

        anchors.fill: parent
        source: root.useOriginalColor ? thresholded : colored
        radius: root.effectiveDiffusion
        transparentBorder: true
        visible: false
    }

    // 4. 強さ調整用レイヤー
    Item {
        id: glowLayer

        anchors.fill: parent
        visible: false
        // 強さを不透明度で表現 (1.0で最大)
        opacity: Math.min(1, root.strength)

        ShaderEffectSource {
            anchors.fill: parent
            sourceItem: blurred
        }

    }

    // 5. 合成 (Blend: Add)
    Blend {
        id: blended

        anchors.fill: parent
        source: root.source
        foregroundSource: glowLayer
        mode: "add"
        visible: !root.fixedSize
    }

    // 6. サイズ固定 (OpacityMaskで元画像の領域で切り抜く)
    OpacityMask {
        anchors.fill: parent
        source: blended
        maskSource: root.source
        visible: root.fixedSize
    }

}
