import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 中心X/Y、幅、高さ、速度、波紋数、波紋間隔、増幅減衰回数
    property real centerX: root.evalNumber("centerX", 0)
    property real centerY: root.evalNumber("centerY", 0)
    property real rippleWidth: Math.max(1, root.evalNumber("width", 15))
    property real rippleHeight: Math.max(0, root.evalNumber("height", 15))
    property real speed: root.evalNumber("speed", 150)
    property real time: root.evalNumber("time", 0)
    property real rippleCount: Math.max(0, root.evalNumber("rippleCount", 0))
    property real rippleInterval: Math.max(1, root.evalNumber("rippleInterval", 10))
    property real amplitudeDecay: root.evalNumber("amplitudeDecay", 0)

    ShaderEffect {
        property variant source: root.sourceProxy
        property real centerX: root.centerX
        property real centerY: root.centerY
        property real rippleWidth: root.rippleWidth
        property real rippleHeight: root.rippleHeight
        property real speed: root.speed
        property real time: root.time
        property real rippleCount: root.rippleCount
        property real rippleInterval: root.rippleInterval
        property real amplitudeDecay: root.amplitudeDecay
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "ripple.frag.qsb"
    }

}
