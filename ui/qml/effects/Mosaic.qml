import QtQuick
import "qrc:/qt/qml/AviQtl/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real size: Math.max(1, root.evalNumber("size", 10))

    ShaderEffect {
        property variant source: root.sourceProxy
        property real size: root.size
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "mosaic.frag.qsb"
    }

}
