import QtQuick
import qrc:/qt/qml/Rinauiqml/common as Common

Common.BaseEffect {
    id: root

    // AviUtl: ずれ幅 0-500、角度 0-360、強さ 0-100
    property real  shiftWidth: Math.max(0, root.evalNumber("shiftWidth", 10))
    property real  angle:      root.evalNumber("angle", 0)
    property real  strength:   Math.max(0, Math.min(100, root.evalNumber("strength", 100)))

    ShaderEffect {
        property variant source:       root.sourceProxy
        property real    shiftWidth:   root.shiftWidth
        property real    angle:        root.angle
        property real    strength:     root.strength
        property real    targetWidth:  root.width
        property real    targetHeight: root.height
        anchors.fill: parent
        fragmentShader: "colorshift.frag.qsb"
    }
}
