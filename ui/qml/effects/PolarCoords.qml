import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 中心幅/拡大率/回転/渦巻
    property real centerWidth: Math.max(0, root.evalNumber("centerWidth", 0))
    property real scaleFactor: Math.max(1, root.evalNumber("scale", 100))
    property real rotation: root.evalNumber("rotation", 0)
    property real swirl: root.evalNumber("swirl", 0)

    ShaderEffect {
        property variant source: root.sourceProxy
        property real centerWidth: root.centerWidth
        property real scaleFactor: root.scaleFactor
        property real rotation: root.rotation
        property real swirl: root.swirl
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "polar.frag.qsb"
    }

}
