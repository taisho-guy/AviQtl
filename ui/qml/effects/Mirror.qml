import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property bool horizontal: root.evalParam("horizontal", false)
    property bool vertical: root.evalParam("vertical", false)
    property real centerX: root.evalNumber("centerX", 0)
    property real centerY: root.evalNumber("centerY", 0)
    property bool invertLuma: root.evalParam("invertLuma", false)
    property bool invertChroma: root.evalParam("invertChroma", false)

    ShaderEffect {
        property variant source: root.sourceProxy
        property real horizontal: root.horizontal ? 1 : 0
        property real vertical: root.vertical ? 1 : 0
        property real centerX: root.centerX
        property real centerY: root.centerY
        property real invertLuma: root.invertLuma ? 1 : 0
        property real invertChroma: root.invertChroma ? 1 : 0
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "mirror.frag.qsb"
    }

}
