import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real angle: root.evalNumber("angle", 0)
    property real length: Math.max(0, root.evalNumber("length", 10))

    ShaderEffect {
        property variant source: root.sourceProxy
        property real angle: root.angle
        property real length: root.length
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "directional_blur.frag.qsb"
    }

}
