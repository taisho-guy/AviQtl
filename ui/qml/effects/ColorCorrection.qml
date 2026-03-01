import QtQuick
import qrc:/qt/qml/Rinauiqml/common as Common

Common.BaseEffect {
    id: root

    // AviUtl: 明るさ/コントラスト/色相/輝度/彩度 各-100~100、飽和する bool
    property real  brightness:  root.evalNumber("brightness",  0) / 100.0
    property real  contrast:    root.evalNumber("contrast",    0) / 100.0
    property real  hue:         root.evalNumber("hue",         0)
    property real  luminance:   root.evalNumber("luminance",   0) / 100.0
    property real  saturation:  root.evalNumber("saturation",  0) / 100.0
    property bool  saturate:    root.evalParam("saturate", false)

    ShaderEffect {
        property variant source:     root.sourceProxy
        property real    brightness: root.brightness
        property real    contrast:   root.contrast
        property real    hue:        root.hue
        property real    luminance:  root.luminance
        property real    saturation: root.saturation
        property real    saturate:   root.saturate ? 1.0 : 0.0
        anchors.fill: parent
        fragmentShader: "colorcorrection.frag.qsb"
    }
}
