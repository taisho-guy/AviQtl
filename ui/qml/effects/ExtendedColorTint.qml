import QtQuick
import qrc:/qt/qml/Rinauiqml/common as Common

Common.BaseEffect {
    id: root

    // AviUtl 拡張色設定: R/G/B 各 0-255 (128=変化なし、0=そのチャンネル消去)
    property real  r: Math.max(0, Math.min(255, root.evalNumber("r", 255)))
    property real  g: Math.max(0, Math.min(255, root.evalNumber("g", 255)))
    property real  b: Math.max(0, Math.min(255, root.evalNumber("b", 255)))

    ShaderEffect {
        property variant source: root.sourceProxy
        property real    rScale: root.r / 255.0
        property real    gScale: root.g / 255.0
        property real    bScale: root.b / 255.0
        anchors.fill: parent
        fragmentShader: "extendedcolortint.frag.qsb"
    }
}
