import Qt5Compat.GraphicalEffects
import QtQuick
import QtQuick.Effects
import QtQuick3D
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    // removed: managed by BaseObject

    id: root

    property string textContent: evalString("text", "text", "テキスト")
    property string fontFamily: evalString("text", "fontFamily", "sans-serif")
    property real fontSize: evalNumber("text", "fontSize", 48)
    property bool fontBold: evalBool("text", "fontBold", false)
    property bool fontItalic: evalBool("text", "fontItalic", false)
    property real letterSpacing: evalNumber("text", "letterSpacing", 0)
    property real lineSpacing: evalNumber("text", "lineSpacing", 0)
    property int alignment: Math.round(evalNumber("text", "alignment", Text.AlignHCenter))
    property color textColor: evalColor("text", "color", "#ffffff")
    property bool outlineEnabled: evalBool("text", "outlineEnabled", false)
    property color outlineColor: evalColor("text", "outlineColor", "#000000")
    property real outlineWidth: evalNumber("text", "outlineWidth", 2)
    property bool shadowEnabled: evalBool("text", "shadowEnabled", false)
    property color shadowColor: evalColor("text", "shadowColor", "#80000000")
    property real shadowOffsetX: evalNumber("text", "shadowOffsetX", 5)
    property real shadowOffsetY: evalNumber("text", "shadowOffsetY", 5)
    property bool bgEnabled: evalBool("text", "bgEnabled", false)
    property color bgColor: evalColor("text", "backgroundColor", "#80000000")
    property real bgRadius: evalNumber("text", "backgroundRadius", 10)
    property real bgPaddingX: evalNumber("text", "backgroundPaddingX", 20)
    property real bgPaddingY: evalNumber("text", "backgroundPaddingY", 10)
    // 縁取りがはみ出さないようにパディングを確保
    readonly property real _pad: outlineEnabled ? Math.ceil(outlineWidth) + 2 : 2

    sourceItem: Item {
        // 【修正】width/height をコンテンツの implicitSize から直接算出し、
        // 親(renderHost)のサイズに一切依存しない自律サイズにする。
        // adopt2D で親が 1920x1080 の renderHost に変わっても再レイアウトが起きない。
        width: Math.max(textItem.implicitWidth + root._pad * 2 + (root.bgEnabled ? root.bgPaddingX * 2 : 0), 1)
        height: Math.max(textItem.implicitHeight + root._pad * 2 + (root.bgEnabled ? root.bgPaddingY * 2 : 0), 1)
        // opacity/visible は BaseObject.onSourceItemChanged が設定する
        // visible: true
        opacity: 1

        Rectangle {
            x: 0
            y: 0
            width: parent.width
            height: parent.height
            visible: root.bgEnabled
            color: root.bgColor
            radius: root.bgRadius
        }

        Item {
            id: textWrapper

            // 【修正】anchors.centerIn を廃止して x/y を明示固定する。
            // anchors.centerIn は親サイズに依存するため reparent 時に循環レイアウトの原因になる。
            x: root._pad + (root.bgEnabled ? root.bgPaddingX : 0)
            y: root._pad + (root.bgEnabled ? root.bgPaddingY : 0)
            width: textItem.implicitWidth + root._pad * 2
            height: textItem.implicitHeight + root._pad * 2

            Text {
                id: textItem

                // 【修正】anchors.centerIn を廃止して x/y を明示固定する。
                x: root._pad
                y: root._pad
                // 【修正】wrapMode を NoWrap に明示して親幅に引きずられないようにする。
                wrapMode: Text.NoWrap
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
                renderType: Text.CurveRendering
            }

            Glow {
                anchors.fill: textItem
                source: textItem
                visible: root.outlineEnabled && root.outlineWidth > 0
                color: root.outlineColor
                radius: Math.ceil(root.outlineWidth)
                samples: Math.min(64, 1 + Math.ceil(root.outlineWidth) * 2)
                spread: 1
                transparentBorder: true
                z: -1
            }

        }

        ShaderEffectSource {
            id: textCapture

            sourceItem: textWrapper
            hideSource: root.shadowEnabled
            live: true
            visible: false
            width: textWrapper.width
            height: textWrapper.height
        }

        MultiEffect {
            x: textWrapper.x
            y: textWrapper.y
            width: textWrapper.width
            height: textWrapper.height
            source: textCapture
            visible: root.shadowEnabled
            shadowEnabled: true
            shadowColor: root.shadowColor
            shadowBlur: 0
            shadowHorizontalOffset: root.shadowOffsetX
            shadowVerticalOffset: root.shadowOffsetY
        }

    }

}
