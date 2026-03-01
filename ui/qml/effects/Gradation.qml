import QtQuick
import qrc:/qt/qml/Rinauiqml/common as Common

Common.BaseEffect {
    id: root

    // AviUtl: 強さ 0-100、中心X/Y、角度 0-360、幅 0-100
    //         形状: 線=0/円=1/四角形=2/凸形=3、開始色、終了色
    property real  strength:    Math.max(0, Math.min(1, root.evalNumber("strength", 100) / 100.0))
    property real  centerX:     root.evalNumber("centerX", 0)
    property real  centerY:     root.evalNumber("centerY", 0)
    property real  angle:       root.evalNumber("angle", 0)
    property real  gradWidth:   Math.max(0, root.evalNumber("gradWidth", 50))
    property real  shape: {
        var s = root.evalParam("shape", "line")
        if (s === "circle")  return 1.0
        if (s === "rect")    return 2.0
        if (s === "convex")  return 3.0
        return 0.0
    }
    property color startColor:  root.evalColor("startColor", "#000000")
    property color endColor:    root.evalColor("endColor",   "#ffffff")

    ShaderEffect {
        property variant source:       root.sourceProxy
        property color   startColor:   root.startColor
        property color   endColor:     root.endColor
        property real    strength:     root.strength
        property real    centerX:      root.centerX
        property real    centerY:      root.centerY
        property real    angle:        root.angle
        property real    gradWidth:    root.gradWidth
        property real    shape:        root.shape
        property real    targetWidth:  root.width
        property real    targetHeight: root.height
        anchors.fill: parent
        fragmentShader: "gradation.frag.qsb"
    }
}
