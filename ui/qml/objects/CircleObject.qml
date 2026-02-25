import QtQuick
import QtQuick3D
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: root

    property real size: evalParam("circle", "size", 100)
    property color color: evalParam("circle", "color", "#ffffff")
    property real opacity: evalParam("circle", "opacity", 1)

    sourceItem: sourceItem

    Item {
        id: sourceItem

        visible: false
        width: root.size + padding * 2
        height: root.size + padding * 2

        Rectangle {
            anchors.centerIn: parent
            width: root.size
            height: root.size
            radius: width / 2
            color: root.color
            antialiasing: true
        }

    }

    Model {
        source: "#Rectangle"
        scale: Qt.vector3d(((renderer.output.sourceItem ? renderer.output.sourceItem.width : sourceItem.width) / 100), ((renderer.output.sourceItem ? renderer.output.sourceItem.height : sourceItem.height) / 100), 1)
        opacity: root.opacity

        materials: DefaultMaterial {
            lighting: DefaultMaterial.NoLighting
            blendMode: root.blendMode
            cullMode: root.cullMode

            diffuseMap: Texture {
                sourceItem: renderer.output
            }

        }

    }

}
