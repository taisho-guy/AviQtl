import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real strength: Math.max(0, root.evalNumber("strength", 100)) / 100
    property real diffusion: Math.max(0, root.evalNumber("diffusion", 10))
    property bool fixedSize: root.evalParam("fixedSize", false)

    // 1. 拡散光層 (背面)
    MultiEffect {
        anchors.fill: parent
        source: root.sourceProxy
        blurEnabled: true
        blurMax: 64
        blur: Math.min(1, root.diffusion / 64)
        colorization: 1
        colorizationColor: "white"
        opacity: root.strength
    }

    // 2. 元画像 (前面)
    ShaderEffect {
        property variant source: root.sourceProxy

        anchors.fill: parent
    }

}
