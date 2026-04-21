import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property int countX: Math.max(1, Math.round(root.evalNumber("countX", 2)))
    property int countY: Math.max(1, Math.round(root.evalNumber("countY", 2)))
    property real intervalX: root.evalNumber("intervalX", 0)
    property real intervalY: root.evalNumber("intervalY", 0)
    property bool mirror: root.evalParam("mirror", false)

    // ループ枚数 × インターバル分だけキャンバスを拡張
    expansion: ({
        "top": Math.max(0, (countY - 1) * intervalY),
        "right": Math.max(0, (countX - 1) * intervalX),
        "bottom": Math.max(0, (countY - 1) * intervalY),
        "left": Math.max(0, (countX - 1) * intervalX)
    })

    ShaderEffect {
        property variant source: root.sourceProxy
        property real countX: root.countX
        property real countY: root.countY
        property real intervalX: root.intervalX
        property real intervalY: root.intervalY
        property real mirror: root.mirror ? 1 : 0
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "image_loop.frag.qsb"
    }

}
