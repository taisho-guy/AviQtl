import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real centerX: root.evalNumber("centerX", 0)
    property real centerY: root.evalNumber("centerY", 0)
    property real angle: root.evalNumber("angle", 0)
    property real clipWidth: root.evalNumber("width", 0)
    property real blur: Math.max(0, root.evalNumber("blur", 0))

    ShaderEffect {
        property variant source: root.sourceProxy
        property real centerX: root.centerX
        property real centerY: root.centerY
        property real angle: root.angle
        property real clipWidth: root.clipWidth
        property real blur: root.blur
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "diagonal_clipping.frag.qsb"
    }

}
