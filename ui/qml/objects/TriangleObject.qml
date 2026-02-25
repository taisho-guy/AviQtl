import QtQuick
import QtQuick.Shapes
import QtQuick3D
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: root

    property real size: evalParam("triangle", "size", 100)
    property color color: evalParam("triangle", "color", "#ffffff")
    property real opacity: evalParam("triangle", "opacity", 1)

    sourceItem: sourceItem

    Item {
        id: sourceItem

        visible: false
        width: root.size + padding * 2
        height: root.size + padding * 2

        Shape {
            anchors.centerIn: parent
            width: root.size
            height: root.size

            ShapePath {
                strokeWidth: 0
                fillColor: root.color
                startX: root.size / 2
                startY: 0

                PathLine {
                    x: root.size
                    y: root.size
                }

                PathLine {
                    x: 0
                    y: root.size
                }

                PathLine {
                    x: root.size / 2
                    y: 0
                }

            }

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
