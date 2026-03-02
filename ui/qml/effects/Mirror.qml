import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 透明度/減衰/境目調整/ミラーの方向 (下/上/右/左)
    property real transparency: Math.max(0, Math.min(100, root.evalNumber("transparency", 0)))
    property real decay: Math.max(0, Math.min(100, root.evalNumber("decay", 0)))
    property real boundary: Math.max(0, root.evalNumber("boundary", 0))
    // direction: 0=下 1=上 2=右 3=左
    property real direction: {
        var d = root.evalParam("direction", "down");
        if (d === "up")
            return 1;

        if (d === "right")
            return 2;

        if (d === "left")
            return 3;

        return 0;
    }

    ShaderEffect {
        property variant source: root.sourceProxy
        property real transparency: root.transparency
        property real decay: root.decay
        property real boundary: root.boundary
        property real direction: root.direction
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "mirror.frag.qsb"
    }

}
