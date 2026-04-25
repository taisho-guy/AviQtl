import QtQuick
import "qrc:/qt/qml/AviQtl/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real radius: Math.max(0, root.evalNumber("radius", 10))
    property color colorVal: root.evalColor("color", "#80000000")
    property real xOffset: root.evalNumber("x", 5)
    property real yOffset: root.evalNumber("y", 5)
    property real opacityVal: Math.max(0, Math.min(100, root.evalNumber("opacity", 100))) / 100

    // offset 方向と radius 分だけ非対称に拡張
    expansion: ({
        "top": Math.max(0, -yOffset) + radius,
        "right": Math.max(0, xOffset) + radius,
        "bottom": Math.max(0, yOffset) + radius,
        "left": Math.max(0, -xOffset) + radius
    })

    ShaderEffect {
        property variant source: root.sourceProxy
        property real radius: root.radius
        property color shadowColor: root.colorVal
        property real xOffset: root.xOffset
        property real yOffset: root.yOffset
        property real strength: root.opacityVal
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "drop_shadow.frag.qsb"
    }

}
