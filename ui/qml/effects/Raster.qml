import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real amplitude: root.evalNumber("amplitude", 10)
    property real frequency: root.evalNumber("frequency", 10)
    property real speed: root.evalNumber("speed", 5)
    property bool vertical: root.evalParam("vertical", false)

    ShaderEffect {
        property variant source: root.sourceProxy
        property real amplitude: root.amplitude
        property real frequency: root.frequency
        property real time: root.frame * (root.speed / 10)
        property real vertical: root.vertical ? 1 : 0
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "raster.frag.qsb"
    }

}
