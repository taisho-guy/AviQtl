import QtQuick
import qrc:/qt/qml/Rinauiqml/common as Common

Common.BaseEffect {
    id: root

    // AviUtl: 強さ/拡散/比率/逆光
    property real  strength:  Math.max(0, root.evalNumber("strength",  100))
    property real  diffusion: Math.max(0, root.evalNumber("diffusion",  50))
    property real  ratio:     Math.max(-100, Math.min(100, root.evalNumber("ratio", 0)))
    property bool  backlight: root.evalParam("backlight", false)

    ShaderEffect {
        property variant source:       root.sourceProxy
        property real    strength:     root.strength
        property real    diffusion:    root.diffusion
        property real    ratio:        root.ratio
        property real    backlight:    root.backlight ? 1.0 : 0.0
        property real    targetWidth:  root.width
        property real    targetHeight: root.height
        anchors.fill: parent
        fragmentShader: "light.frag.qsb"
    }
}
