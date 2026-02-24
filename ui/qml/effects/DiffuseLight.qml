import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real strength: Math.max(0, root.evalNumber("strength", 100)) / 100
    property real diffusion: Math.max(0, root.evalNumber("diffusion", 10))
    property bool fixedSize: root.evalParam("fixedSize", false)

    // Use shadow to simulate diffuse light (glow)
    shadowEnabled: true
    shadowColor: Qt.rgba(1, 1, 1, root.strength) // White glow with strength
    shadowBlur: root.diffusion / 64 // Approximate normalization
    shadowHorizontalOffset: 0
    shadowVerticalOffset: 0
    maskEnabled: root.fixedSize
    maskSource: root.source
}
