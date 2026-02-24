import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real topVal: Math.max(0, root.evalNumber("top", 0))
    property real bottomVal: Math.max(0, root.evalNumber("bottom", 0))
    property real leftVal: Math.max(0, root.evalNumber("left", 0))
    property real rightVal: Math.max(0, root.evalNumber("right", 0))
    property bool centerVal: root.evalParam("center", false)

    // マスク適用
    maskEnabled: true
    maskSource: mask
    visible: root.source !== null

    // マスク画像の生成 (白 = 表示、透明 = 非表示)
    Item {
        id: mask

        anchors.fill: parent
        visible: false
        layer.enabled: true // テクスチャとしてキャッシュ

        Rectangle {
            x: root.leftVal
            y: root.topVal
            width: Math.max(0, parent.width - root.leftVal - root.rightVal)
            height: Math.max(0, parent.height - root.topVal - root.bottomVal)
            color: "white"
        }

    }

    // 中心位置の補正
    // 左を削ると見た目の中心は右にずれるため、左へ移動させて補正する
    // 補正量 X = (右カット量 - 左カット量) / 2
    transform: Translate {
        x: root.centerVal ? (root.rightVal - root.leftVal) / 2 : 0
        y: root.centerVal ? (root.bottomVal - root.topVal) / 2 : 0
    }

}
