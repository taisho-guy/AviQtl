import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real samples: Math.max(1, root.evalNumber("samples", 10))
    property real strength: root.evalNumber("strength", 0.1)
    property real centerX: root.evalNumber("x", 0)
    property real centerY: root.evalNumber("y", 0)

    ShaderEffect {
        property variant source: root.sourceProxy
        property real samples: root.samples
        property real strength: root.strength
        property real centerX: root.centerX
        property real centerY: root.centerY
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "radial_blur.frag.qsb"
    }

}
