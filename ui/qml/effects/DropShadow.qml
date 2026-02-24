import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real radius: Math.max(0, root.evalNumber("radius", 10))
    property color color: root.evalColor("color", "#80000000")
    property real xOffset: root.evalNumber("x", 5)
    property real yOffset: root.evalNumber("y", 5)
    property real opacityVal: Math.max(0, Math.min(100, root.evalNumber("opacity", 100))) / 100

    // 1. 影 (背面)
    MultiEffect {
        anchors.fill: parent
        source: root.sourceProxy
        blurEnabled: true
        blurMax: 100
        blur: Math.min(1, root.radius / 100)
        colorization: 1
        colorizationColor: root.color
        opacity: root.opacityVal

        transform: Translate {
            x: root.xOffset
            y: root.yOffset
        }

    }

    // 2. 元画像 (前面)
    ShaderEffect {
        property variant source: root.sourceProxy

        anchors.fill: parent
    }

}
