import QtQuick
import QtQuick.Effects
import QtQuick3D
import Rina
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: root

    // キーフレーム対応プロパティ（rect エフェクトから取得）
    property real sizeW: evalParam("rect", "sizeW", 100)
    property real sizeH: evalParam("rect", "sizeH", 100)
    property color color: evalParam("rect", "color", "#66aa99")
    property real opacity: evalParam("rect", "opacity", 1)

    sourceItem: sourceItem

    // ソースアイテム（BaseObject.padding を使用）
    Item {
        id: sourceItem

        visible: false
        width: root.sizeW + padding * 2
        height: root.sizeH + padding * 2

        Rectangle {
            anchors.centerIn: parent
            width: root.sizeW
            height: root.sizeH
            color: root.color
        }

    }

    Model {
        source: "#Rectangle"
        // 3Dモデルへのマッピング
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
