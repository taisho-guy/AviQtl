import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property bool horizontal: root.evalParam("horizontal", false)
    property bool vertical: root.evalParam("vertical", false)

    ShaderEffect {
        property variant source: root.sourceProxy

        anchors.fill: parent

        transform: Scale {
            origin.x: parent.width / 2
            origin.y: parent.height / 2
            xScale: root.horizontal ? -1 : 1
            yScale: root.vertical ? -1 : 1
        }

    }

}
