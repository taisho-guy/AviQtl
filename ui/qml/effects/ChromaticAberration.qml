import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real redX: root.evalNumber("redX", 5)
    property real redY: root.evalNumber("redY", 0)
    property real greenX: root.evalNumber("greenX", 0)
    property real greenY: root.evalNumber("greenY", 0)
    property real blueX: root.evalNumber("blueX", -5)
    property real blueY: root.evalNumber("blueY", 0)
    // 各チャンネルの最大オフセットで非対称拡張
    readonly property real _maxX: Math.max(Math.abs(redX), Math.abs(greenX), Math.abs(blueX))
    readonly property real _maxY: Math.max(Math.abs(redY), Math.abs(greenY), Math.abs(blueY))

    expansion: ({
        "top": _maxY,
        "right": _maxX,
        "bottom": _maxY,
        "left": _maxX
    })

    ShaderEffect {
        property variant source: root.sourceProxy
        property vector2d redOffset: Qt.vector2d(root.redX, root.redY)
        property vector2d greenOffset: Qt.vector2d(root.greenX, root.greenY)
        property vector2d blueOffset: Qt.vector2d(root.blueX, root.blueY)
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "chromatic_aberration.frag.qsb"
    }

}
