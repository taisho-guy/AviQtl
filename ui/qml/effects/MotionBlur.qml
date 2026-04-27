import QtQuick
import "qrc:/qt/qml/AviQtl/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 間隔(shutterSpeed), 分解能(quality), 残像(trail)
    // AviQtl追加: velX/velY でブラー方向を手動指定
    property real interval: Math.max(0, Math.min(100, root.evalNumber("shutterSpeed", 50)))
    property real quality: Math.max(1, Math.min(100, root.evalNumber("quality", 16)))
    property bool trail: root.evalParam("trail", false)
    property real velX: root.evalNumber("velX", 0)
    property real velY: root.evalNumber("velY", 0)

    ShaderEffect {
        property variant source: root.sourceProxy
        property real quality: root.quality
        property real shutterSpeed: root.interval
        property real velX: root.velX
        property real velY: root.velY
        property real trail: root.trail ? 1 : 0
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "motionblur.frag.qsb"
    }

}
