import QtQuick
import qrc:/qt/qml/Rinauiqml/common as Common

Common.BaseEffect {
    id: root

    property real size:      Math.max(0, root.evalNumber("size", 10))
    property real aspect:    Math.max(-100, Math.min(100, root.evalNumber("aspect", 0)))
    property real gain:      Math.max(0, root.evalNumber("gain", 0) / 100.0)
    property bool fixedSize: root.evalParam("fixedSize", false)

    readonly property real hRatio: aspect > 0 ? (100 - aspect) / 100 : 1.0
    readonly property real vRatio: aspect < 0 ? (100 + aspect) / 100 : 1.0
    readonly property real hLen:   size * hRatio
    readonly property real vLen:   size * vRatio

    // パス1: 水平方向ブラー
    ShaderEffect {
        id: hPass
        property variant source:      root.sourceProxy
        property vector2d direction:  Qt.vector2d(1, 0)
        property real     radius:     root.hLen
        property real     gain:       0.0
        property real     targetWidth: root.width
        property real     targetHeight: root.height
        anchors.fill: parent
        visible: false
        fragmentShader: "blur.frag.qsb"
    }
    ShaderEffectSource {
        id: hPassProxy
        sourceItem: hPass
        hideSource: false
        visible: false
        live: true
    }

    // パス2: 垂直方向ブラー (fixedSize=false → これが出力)
    ShaderEffect {
        id: vPass
        property variant source:       hPassProxy
        property vector2d direction:   Qt.vector2d(0, 1)
        property real     radius:      root.vLen
        property real     gain:        root.gain
        property real     targetWidth: root.width
        property real     targetHeight: root.height
        anchors.fill: parent
        visible: !root.fixedSize
        fragmentShader: "blur.frag.qsb"
    }
    ShaderEffectSource {
        id: vPassProxy
        sourceItem: vPass
        hideSource: false
        visible: false
        live: true
    }

    // パス3: マスク (fixedSize=true → 元の輪郭に収める)
    ShaderEffect {
        id: maskPass
        property variant source:     vPassProxy
        property variant maskSource: root.sourceProxy
        anchors.fill: parent
        visible: root.fixedSize
        fragmentShader: "mask.frag.qsb"
    }
}
