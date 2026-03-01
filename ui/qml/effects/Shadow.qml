import QtQuick
import qrc:/qt/qml/Rinauiqml/common as Common

Common.BaseEffect {
    id: root

    // AviUtl: X/Y オフセット、濃さ、拡散、影を別オブジェクトとして表示
    property real  offsetX:     root.evalNumber("x",        5)
    property real  offsetY:     root.evalNumber("y",        5)
    property real  density:     Math.max(0, Math.min(1, root.evalNumber("density",   80) / 100.0))
    property real  diffusion:   Math.max(0, root.evalNumber("diffusion",  5))
    property bool  shadowOnly:  root.evalParam("shadowOnly", false)

    // パス1: オフセット + アルファ抽出 (黒影)
    ShaderEffect {
        id: shadowGenPass
        property variant source:       root.sourceProxy
        property real    offsetX:      root.offsetX
        property real    offsetY:      root.offsetY
        property real    density:      root.density
        property real    targetWidth:  root.width
        property real    targetHeight: root.height
        anchors.fill: parent
        visible: false
        fragmentShader: "shadow_gen.frag.qsb"
    }
    ShaderEffectSource {
        id: shadowGenProxy
        sourceItem: shadowGenPass
        hideSource: false; visible: false; live: true
    }

    // パス2/3: 拡散 (Gaussian ブラー)
    ShaderEffect {
        id: shadowHBlur
        property variant source:       shadowGenProxy
        property vector2d direction:   Qt.vector2d(1, 0)
        property real     radius:      root.diffusion
        property real     gain:        0.0
        property real     targetWidth: root.width
        property real     targetHeight: root.height
        anchors.fill: parent
        visible: false
        fragmentShader: "blur.frag.qsb"
    }
    ShaderEffectSource {
        id: shadowHProxy
        sourceItem: shadowHBlur
        hideSource: false; visible: false; live: true
    }
    ShaderEffect {
        id: shadowVBlur
        property variant source:       shadowHProxy
        property vector2d direction:   Qt.vector2d(0, 1)
        property real     radius:      root.diffusion
        property real     gain:        0.0
        property real     targetWidth: root.width
        property real     targetHeight: root.height
        anchors.fill: parent
        visible: false
        fragmentShader: "blur.frag.qsb"
    }
    ShaderEffectSource {
        id: shadowBlurredProxy
        sourceItem: shadowVBlur
        hideSource: false; visible: false; live: true
    }

    // パス4: 影をオブジェクトの後ろに合成
    ShaderEffect {
        property variant source:       root.sourceProxy
        property variant shadowSource: shadowBlurredProxy
        property real    shadowOnly:   root.shadowOnly ? 1.0 : 0.0
        anchors.fill: parent
        fragmentShader: "shadow_composite.frag.qsb"
    }
}
