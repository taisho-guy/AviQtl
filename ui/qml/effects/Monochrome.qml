import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property color color: root.evalColor("color", "#ffffff")
    property real strength: Math.max(0, Math.min(100, root.evalNumber("strength", 100))) / 100

    // MultiEffectの標準描画を無効化
    maskEnabled: true
    maskSource: emptyMask

    MultiEffect {
        anchors.fill: parent
        source: root.sourceProxy
        colorization: root.strength
        colorizationColor: root.color
    }

}
