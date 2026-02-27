import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real topVal: Math.max(0, root.evalNumber("top", 0))
    property real bottomVal: Math.max(0, root.evalNumber("bottom", 0))
    property real leftVal: Math.max(0, root.evalNumber("left", 0))
    property real rightVal: Math.max(0, root.evalNumber("right", 0))
    property bool centerVal: root.evalParam("center", false)

    ShaderEffect {
        property variant source: root.sourceProxy
        property real topVal: root.topVal
        property real bottomVal: root.bottomVal
        property real leftVal: root.leftVal
        property real rightVal: root.rightVal
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "clipping.frag.qsb"
    }

    transform: Translate {
        x: root.centerVal ? (root.rightVal - root.leftVal) / 2 : 0
        y: root.centerVal ? (root.bottomVal - root.topVal) / 2 : 0
    }

}
