import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real strength: Math.max(0, root.evalNumber("strength", 10))
    property real centerX: root.evalNumber("x", 0)
    property real centerY: root.evalNumber("y", 0)
    property int mode: {
        var m = root.evalParam("mode", "前方に合成");
        if (m === "前方に合成")
            return 0;

        if (m === "後方に合成")
            return 1;

        if (m === "光成分のみ")
            return 2;

        return Number(m) || 0;
    }
    property bool useOriginalColor: root.evalParam("useOriginalColor", true)
    property color flashColor: root.evalColor("color", "#ffffff")
    property bool fixedSize: root.evalParam("fixedSize", false)

    ShaderEffect {
        property variant source: root.sourceProxy
        property real strength: root.strength
        property real centerX: root.centerX
        property real centerY: root.centerY
        property real mode: root.mode
        property real useOriginalColor: root.useOriginalColor ? 1 : 0
        property color flashColor: root.flashColor
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "flash.frag.qsb"
    }

}
