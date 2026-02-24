import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property bool horizontal: root.evalParam("horizontal", false)
    property bool vertical: root.evalParam("vertical", false)

    // MultiEffect自体には反転プロパティがないため、Itemのtransformを使用
    transform: Scale {
        origin.x: root.width / 2
        origin.y: root.height / 2
        xScale: root.horizontal ? -1 : 1
        yScale: root.vertical ? -1 : 1
    }

}
