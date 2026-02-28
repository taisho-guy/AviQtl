import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real radius: Math.max(0, root.evalNumber("radius", 10))
    property real brightness: Math.max(0, root.evalNumber("brightness", 0))

    ShaderEffect {
        property variant source: root.sourceProxy
        property real radius: root.radius
        property real brightness: root.brightness
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "lens_blur.frag.qsb"
    }

}
