import Qt5Compat.GraphicalEffects 1.0
import QtQuick 2.15
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real strength: Math.max(0, root.evalNumber("strength", 100)) / 100
    property real diffusion: Math.max(0, root.evalNumber("diffusion", 10))
    property bool fixedSize: root.evalParam("fixedSize", false)

    // 1. 拡散 (ぼかし)
    FastBlur {
        id: blurred

        anchors.fill: parent
        source: root.source
        radius: root.diffusion
        transparentBorder: true
        visible: false
    }

    // 2. 強さ調整用レイヤー
    Item {
        id: lightLayer

        anchors.fill: parent
        visible: false
        opacity: root.strength

        ShaderEffectSource {
            anchors.fill: parent
            sourceItem: blurred
        }

    }

    // 3. 合成 (Blend: Add)
    Blend {
        id: blended

        anchors.fill: parent
        source: root.source
        foregroundSource: lightLayer
        mode: "add"
        visible: !root.fixedSize
    }

    // 4. サイズ固定 (OpacityMaskで元画像の領域で切り抜く)
    OpacityMask {
        anchors.fill: parent
        source: blended
        maskSource: root.source
        visible: root.fixedSize
    }

}
