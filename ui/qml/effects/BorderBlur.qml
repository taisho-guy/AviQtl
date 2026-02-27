import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real size: Math.max(0, root.evalNumber("size", 5))
    property real aspect: Math.max(-100, Math.min(100, root.evalNumber("aspect", 0)))
    property bool blurAlpha: root.evalParam("blur_alpha", true)

    ShaderEffect {
        property variant source: root.sourceProxy
        property real size: root.size
        property real targetWidth: root.width
        property real targetHeight: root.height
        property real blurAlpha: root.blurAlpha ? 1 : 0

        anchors.fill: parent
        fragmentShader: "border_blur.frag.qsb"
    }

}
