import QtQuick
import "qrc:/qt/qml/AviQtl/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real size: Math.max(0, root.evalNumber("size", 5))
    property real aspect: Math.max(-100, Math.min(100, root.evalNumber("aspect", 0)))
    property bool blurAlpha: root.evalParam("blur_alpha", true)
    readonly property real hRatio: aspect > 0 ? (100 - aspect) / 100 : 1
    readonly property real vRatio: aspect < 0 ? (100 + aspect) / 100 : 1
    readonly property real effectSize: size * Math.max(hRatio, vRatio)

    ShaderEffect {
        property variant source: root.sourceProxy
        property real size: root.effectSize
        property real blurAlpha: root.blurAlpha ? 1 : 0
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "borderblur.frag.qsb"
    }

}
