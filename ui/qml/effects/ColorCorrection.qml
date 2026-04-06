import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real brightness: root.evalNumber("brightness", 100) / 100 - 1
    property real contrast: root.evalNumber("contrast", 100) / 100 - 1
    property real hue: root.evalNumber("hue", 0)
    property real luminance: root.evalNumber("lightness", 100) / 100 - 1
    property real saturation: root.evalNumber("saturation", 100) / 100 - 1
    property bool saturate: root.evalParam("saturate", false)

    ShaderEffect {
        property variant source: root.sourceProxy
        property real brightness: root.brightness
        property real contrast: root.contrast
        property real hue: root.hue
        property real luminance: root.luminance
        property real saturation: root.saturation
        property real saturate: root.saturate ? 1 : 0

        anchors.fill: parent
        fragmentShader: "colorcorrection.frag.qsb"
    }

}
