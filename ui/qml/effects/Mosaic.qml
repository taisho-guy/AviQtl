import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real size: Math.max(1, root.evalNumber("size", 10))

    maskEnabled: true
    maskSource: emptyMask

    // MultiEffectの標準描画（ソースのそのままの表示）を無効化するため、
    // 透明なマスクを適用して隠す。
    // これにより、子要素であるShaderEffectSource（モザイク）のみが表示される。
    Item {
        id: emptyMask

        anchors.fill: parent
        visible: false
    }

    ShaderEffect {
        property variant source: root.source
        property real size: root.size

        anchors.fill: parent
        fragmentShader: "mosaic.frag.qsb"
    }

}
