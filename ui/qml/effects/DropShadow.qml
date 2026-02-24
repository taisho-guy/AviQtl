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

    shadowEnabled: true
    // MultiEffectのshadowBlurは0.0-1.0の範囲。100を最大として正規化
    shadowBlur: Math.min(1, root.radius / 100)
    shadowColor: root.color
    shadowHorizontalOffset: root.xOffset
    shadowVerticalOffset: root.yOffset
    shadowOpacity: root.opacityVal
}
