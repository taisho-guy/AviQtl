import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 強さ 0-100、色 (デフォルト黒 → グレースケール)
    property real strength: Math.max(0, Math.min(1, root.evalNumber("strength", 100) / 100))
    property color monoColor: root.evalColor("color", "#ffffff")

    ShaderEffect {
        property variant source: root.sourceProxy
        property real strength: root.strength
        property color monoColor: root.monoColor

        anchors.fill: parent
        fragmentShader: "monochrome.frag.qsb"
    }

}
