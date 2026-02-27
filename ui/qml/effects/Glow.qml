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
    property real fluctuation: {
        if (speed <= 0)
            return 1;

        var t = root.frame * speed * 0.05;
        return 1 + (Math.sin(t) * 0.2 + Math.cos(t * 2.3) * 0.1);
    }
    property real effectiveDiffusion: Math.max(0, diffusion * fluctuation)

    ShaderEffect {
        property variant source: root.sourceProxy
        property real strength: root.strength
        property real diffusion: root.effectiveDiffusion
        property real threshold: root.threshold
        property real useOriginalColor: root.useOriginalColor ? 1 : 0
        property color glowColor: root.glowColor
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "glow.frag.qsb"
    }

}
