import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: サイズ 0-100、ぼかし 0-100、縁色
    property real edgeSize: Math.max(1, root.evalNumber("size", 4))
    property real blur: Math.max(0, root.evalNumber("blur", 0))
    property color edgeColor: root.evalColor("color", "#000000")

    ShaderEffect {
        property variant source: root.sourceProxy
        property real edgeSize: root.edgeSize
        property real blur: root.blur
        property color edgeColor: root.edgeColor
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "edging.frag.qsb"
    }

}
