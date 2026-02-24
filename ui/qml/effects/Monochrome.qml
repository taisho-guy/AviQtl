import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property color color: root.evalColor("color", "#ffffff")
    property real strength: Math.max(0, Math.min(100, root.evalNumber("strength", 100))) / 100

    colorization: root.strength
    colorizationColor: root.color
}
