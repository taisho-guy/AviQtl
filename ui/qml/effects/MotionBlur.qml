import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 間隔(shutterSpeed), 分解能(quality), 残像(trail)
    // Rina追加: velX/velY でブラー方向を手動指定
    property real interval: Math.max(0, Math.min(100, root.evalNumber("shutterSpeed", 50)))
    property real quality: Math.max(1, Math.min(100, root.evalNumber("quality", 16)))
    property bool trail: root.evalParam("trail", false)
    property real velX: root.evalNumber("velX", 0)
    property real velY: root.evalNumber("velY", 0)
    // 速度ベクトル方向に拡張（interval/100 でシャッタースケーリング）
    readonly property real _ex: Math.abs(velX) * interval / 100
    readonly property real _ey: Math.abs(velY) * interval / 100

    expansion: ({
        "top": velY < 0 ? _ey : 0,
        "right": velX > 0 ? _ex : 0,
        "bottom": velY > 0 ? _ey : 0,
        "left": velX < 0 ? _ex : 0
    })

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
