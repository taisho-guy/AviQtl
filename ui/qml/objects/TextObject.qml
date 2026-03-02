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
    property real shadowOffsetX: Number(evalParam("text", "shadowOffsetX", 5))
    property real shadowOffsetY: Number(evalParam("text", "shadowOffsetY", 5))
    property bool bgEnabled: Boolean(evalParam("text", "bgEnabled", false))
    property color bgColor: evalParam("text", "backgroundColor", "#80000000")
    property real bgRadius: Number(evalParam("text", "backgroundRadius", 10))
    property real bgPaddingX: Number(evalParam("text", "backgroundPaddingX", 20))
    property real bgPaddingY: Number(evalParam("text", "backgroundPaddingY", 10))
    // 縁取りがはみ出さないようにパディングを確保
    readonly property real _pad: outlineEnabled ? Math.ceil(outlineWidth) + 2 : 2

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
        // _pad 分 + 背景パディング分だけ拡大
        width: Math.max(textItem.implicitWidth + root._pad * 2 + (root.bgEnabled ? root.bgPaddingX * 2 : 0), 1)
        height: Math.max(textItem.implicitHeight + root._pad * 2 + (root.bgEnabled ? root.bgPaddingY * 2 : 0), 1)
        visible: false

        Rectangle {
            anchors.fill: parent
            visible: root.bgEnabled
            color: root.bgColor
            radius: root.bgRadius
        }

        // テキスト＋縁取りをまとめるコンテナ
        Item {
            id: textWrapper

            anchors.centerIn: parent
            width: textItem.implicitWidth + root._pad * 2
            height: textItem.implicitHeight + root._pad * 2

            // Bug2修正: Canvas による可変幅縁取り
            // ctx.lineWidth = outlineWidth * 2 で幅を制御できる
            Canvas {
                id: outlineCanvas

                anchors.fill: parent
                visible: root.outlineEnabled && root.outlineWidth > 0
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    var ctx2d = getContext("2d");
                    ctx2d.clearRect(0, 0, width, height);
                    if (!root.outlineEnabled || root.outlineWidth <= 0)
                        return ;

                    var wt = root.fontBold ? "bold" : "normal";
                    var st = root.fontItalic ? "italic" : "normal";
                    var lsp = root.letterSpacing !== 0 ? " letter-spacing: " + root.letterSpacing + "px;" : "";
                    ctx2d.font = st + " " + wt + " " + root.fontSize + "px '" + root.fontFamily + "'";
                    ctx2d.strokeStyle = root.outlineColor.toString();
                    ctx2d.lineWidth = root.outlineWidth * 2;
                    ctx2d.lineJoin = "round";
                    ctx2d.lineCap = "round";
                    var alignStr;
                    if (root.alignment === Text.AlignLeft)
                        alignStr = "left";
                    else if (root.alignment === Text.AlignRight)
                        alignStr = "right";
                    else
                        alignStr = "center";
                    ctx2d.textAlign = alignStr;
                    ctx2d.textBaseline = "alphabetic";
                    var xPos = (alignStr === "left") ? root._pad : (alignStr === "right") ? width - root._pad : width / 2;
                    var lines = root.textContent.split("\n");
                    var lh = textItem.lineCount > 0 ? textItem.contentHeight / textItem.lineCount : 0;
                    var baselineY = textItem.y + textItem.baselineOffset;
                    for (var i = 0; i < lines.length; i++) {
                        ctx2d.strokeText(lines[i], xPos, baselineY + i * lh);
                    }
                }

                Connections {
                    function onOutlineWidthChanged() {
                        outlineCanvas.requestPaint();
                    }

                    function onOutlineColorChanged() {
                        outlineCanvas.requestPaint();
                    }

                    function onOutlineEnabledChanged() {
                        outlineCanvas.requestPaint();
                    }

                    function onTextContentChanged() {
                        outlineCanvas.requestPaint();
                    }

                    function onFontFamilyChanged() {
                        outlineCanvas.requestPaint();
                    }

                    function onFontSizeChanged() {
                        outlineCanvas.requestPaint();
                    }

                    function onFontBoldChanged() {
                        outlineCanvas.requestPaint();
                    }

                    function onFontItalicChanged() {
                        outlineCanvas.requestPaint();
                    }

                    function onAlignmentChanged() {
                        outlineCanvas.requestPaint();
                    }

                    function onLineSpacingChanged() {
                        outlineCanvas.requestPaint();
                    }

                    target: root
                }

            }

            // メインテキスト (縁取りは Canvas に任せ style は Normal 固定)
            Text {
                id: textItem

                anchors.centerIn: parent
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
                // Bug2修正: Text.Outline はスタイル幅を制御できないため Normal に変更
                // 縁取りは上の Canvas が担当する
                style: Text.Normal
            }

        }

        // Bug1修正: ShaderEffectSource で textWrapper をキャプチャする
        // hideSource: true にすると textWrapper の直接描画を非表示にしつつ
        // FBO にレンダリングし続けるため、MultiEffect のソースとして使える
        ShaderEffectSource {
            id: textCapture

            sourceItem: textWrapper
            hideSource: root.shadowEnabled
            live: true
            visible: false
            width: textWrapper.width
            height: textWrapper.height
        }

        // 影エフェクト: textCapture (FBOキャプチャ済み) をソースに使う
        // これにより textWrapper が非表示でも正しく影付きテキストが描画される
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
