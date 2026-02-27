import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property color monoColor: root.evalColor("color", "#ffffff")
    property real strength: Math.max(0, Math.min(100, root.evalNumber("strength", 100))) / 100

    ShaderEffect {
        property variant source: root.sourceProxy
        property color monoColor: root.monoColor
        property real strength: root.strength

        anchors.fill: parent
        fragmentShader: "monochrome.frag.qsb"
    }

}
