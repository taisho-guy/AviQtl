import QtQuick
import QtQuick.Effects
import QtQuick3D
import Rina
import "qrc:/qt/qml/Rina/ui/qml/common" as Common
import "qrc:/qt/qml/Rina/ui/qml/common/Logger.js" as Logger

Common.BaseObject {
    id: root

    property string textContent: String(evalParam("text", "text", "Text"))
    property int textSize: Number(evalParam("text", "textSize", 64))
    property color color: evalParam("text", "color", "#ffffff")
    property real opacity: 1

    function dbg(msg) {
        Logger.log("[TextObject][clipId=" + clipId + "] " + msg, TimelineBridge);
    }

    sourceItem: originalSource

    // ソースアイテム
    Item {
        id: originalSource

        visible: false
        width: textItem.implicitWidth + (padding * 4)
        height: textItem.implicitHeight + (padding * 4)

        Text {
            id: textItem

            anchors.centerIn: parent
            text: root.textContent
            font.pixelSize: root.textSize
            color: root.color
            style: Text.Outline
            styleColor: "black"
            renderType: Text.QtRendering
            antialiasing: true
        }

    }

    Model {
        source: "#Rectangle"
        scale: Qt.vector3d(((renderer.output.sourceItem ? renderer.output.sourceItem.width : originalSource.width) / 100), ((renderer.output.sourceItem ? renderer.output.sourceItem.height : originalSource.height) / 100), 1)
        opacity: root.opacity

        materials: DefaultMaterial {
            lighting: DefaultMaterial.NoLighting
            blendMode: DefaultMaterial.SourceOver

            diffuseMap: Texture {
                sourceItem: renderer.output
            }

        }

    }

}
