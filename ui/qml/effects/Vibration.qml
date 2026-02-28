import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real xStrength: root.evalNumber("x", 10)
    property real yStrength: root.evalNumber("y", 10)
    property real speed: root.evalNumber("speed", 20)
    property real randomSeed: root.evalNumber("seed", 0)

    ShaderEffect {
        property variant source: root.sourceProxy
        property real xStrength: root.xStrength
        property real yStrength: root.yStrength
        property real time: root.frame * (root.speed / 10) + root.randomSeed
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "vibration.frag.qsb"
    }

}
