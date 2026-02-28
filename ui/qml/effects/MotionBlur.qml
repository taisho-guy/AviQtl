import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real quality: Math.max(1, root.evalNumber("quality", 16))
    property real shutterSpeed: Math.max(0, root.evalNumber("shutterSpeed", 50))
    // BaseObjectから注入されるプロパティ
    property real clipNodeScaleX: 1
    property real clipNodeScaleY: 1
    property real clipNodePosX: 0
    property real clipNodePosY: 0
    property real clipNodeRotZ: 0
    property real clipNodeOpacity: 1

    ShaderEffect {
        property variant source: root.sourceProxy
        property real quality: root.quality
        property real shutterSpeed: root.shutterSpeed
        property real prevFrame: root.frame - 1
        property real currentFrame: root.frame
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "motion_blur.frag.qsb"
    }

}
