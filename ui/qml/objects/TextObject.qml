import QtQuick
import QtQuick.Effects
import QtQuick3D
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: root

    property string textContent: String(evalParam("text", "text", "テキスト"))
    property string fontFamily: String(evalParam("text", "fontFamily", "sans-serif"))
    property real fontSize: Number(evalParam("text", "fontSize", 48))
    property bool fontBold: Boolean(evalParam("text", "fontBold", false))
    property bool fontItalic: Boolean(evalParam("text", "fontItalic", false))
    property real letterSpacing: Number(evalParam("text", "letterSpacing", 0))
    property real lineSpacing: Number(evalParam("text", "lineSpacing", 0))
    property int alignment: Math.round(Number(evalParam("text", "alignment", Text.AlignHCenter)))
    property color textColor: evalParam("text", "color", "#ffffff")
    property bool outlineEnabled: Boolean(evalParam("text", "outlineEnabled", false))
    property color outlineColor: evalParam("text", "outlineColor", "#000000")
    property real outlineWidth: Number(evalParam("text", "outlineWidth", 2))
    property bool shadowEnabled: Boolean(evalParam("text", "shadowEnabled", false))
    property color shadowColor: evalParam("text", "shadowColor", "#80000000")
    property real shadowBlur: Number(evalParam("text", "shadowBlur", 0.5))
    property real shadowOffsetX: Number(evalParam("text", "shadowOffsetX", 5))
    property real shadowOffsetY: Number(evalParam("text", "shadowOffsetY", 5))
    property bool bgEnabled: Boolean(evalParam("text", "bgEnabled", false))
    property color bgColor: evalParam("text", "backgroundColor", "#80000000")
    property real bgRadius: Number(evalParam("text", "backgroundRadius", 10))
    property real bgPaddingX: Number(evalParam("text", "backgroundPaddingX", 20))
    property real bgPaddingY: Number(evalParam("text", "backgroundPaddingY", 10))

    Model {
        source: "#Rectangle"
        scale: Qt.vector3d((renderer.output.sourceItem ? renderer.output.sourceItem.width : 1) / 100, (renderer.output.sourceItem ? renderer.output.sourceItem.height : 1) / 100, 1)

        materials: DefaultMaterial {
            lighting: DefaultMaterial.NoLighting
            blendMode: root.blendMode
            cullMode: root.cullMode

            diffuseMap: Texture {
                sourceItem: renderer.output
            }

        }

    }

    sourceItem: Item {
        width: Math.max(textItem.implicitWidth + (bgEnabled ? bgPaddingX * 2 : 0), 1)
        height: Math.max(textItem.implicitHeight + (bgEnabled ? bgPaddingY * 2 : 0), 1)
        visible: false

        Rectangle {
            id: backgroundRect

            anchors.fill: parent
            visible: root.bgEnabled
            color: root.bgColor
            radius: root.bgRadius
        }

        Item {
            id: textContainer

            anchors.centerIn: parent
            width: textItem.implicitWidth
            height: textItem.implicitHeight

            Text {
                id: textItem

                text: root.textContent
                font.family: root.fontFamily
                font.pixelSize: root.fontSize
                font.bold: root.fontBold
                font.italic: root.fontItalic
                font.weight: root.fontBold ? Font.Bold : Font.Normal
                font.letterSpacing: root.letterSpacing
                lineHeight: root.lineSpacing > 0 ? root.lineSpacing : 1
                lineHeightMode: root.lineSpacing > 0 ? Text.FixedHeight : Text.ProportionalHeight
                horizontalAlignment: root.alignment
                verticalAlignment: Text.AlignVCenter
                color: root.textColor
                style: root.outlineEnabled ? Text.Outline : Text.Normal
                styleColor: root.outlineColor
                visible: !root.shadowEnabled
            }

        }

        MultiEffect {
            source: textContainer
            anchors.fill: textContainer
            visible: root.shadowEnabled
            shadowEnabled: true
            shadowColor: root.shadowColor
            shadowBlur: root.shadowBlur
            shadowHorizontalOffset: root.shadowOffsetX
            shadowVerticalOffset: root.shadowOffsetY
        }

    }

}
