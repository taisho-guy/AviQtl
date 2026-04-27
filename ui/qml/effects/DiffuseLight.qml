import QtQuick
import "qrc:/qt/qml/AviQtl/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real strength: Math.max(0, root.evalNumber("strength", 100)) / 100
    property real diffusion: Math.max(0, root.evalNumber("diffusion", 10))
    property bool fixedSize: root.evalParam("fixedSize", false)

    ShaderEffect {
        property variant source: root.sourceProxy
        property real strength: root.strength
        property real diffusion: root.diffusion
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "diffuse_light.frag.qsb"
    }

}
