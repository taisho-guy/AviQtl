import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../common" as Common

Rectangle {
    id: rulerRoot
    
    // 外部から注入される依存プロパティ
    property var targetFlickable: null
    property int rulerHeight: 32
    property int timeWidth: 60
    property double fps: 60.0
    property alias canvas: rulerCanvas

    Layout.fillWidth: true
    Layout.preferredHeight: rulerHeight
    color: palette.window
    z: 10

    // ユーティリティ
    function pxToFrame(px, contentX) {
        var scale = TimelineBridge ? TimelineBridge.timelineScale : 1
        var x = px + contentX
        return Math.max(0, Math.round((x - timeWidth) / scale))
    }

    function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }

    function zoomAt(wheel, zoomFactor) {
        if (!TimelineBridge || !targetFlickable) return;
        var oldScale = TimelineBridge.timelineScale;
        var newScale = clamp(oldScale * zoomFactor, 0.1, 20.0);
        if (Math.abs(newScale - oldScale) < 1e-6) return;

        var mouseX = wheel.x !== undefined ? wheel.x : (wheel.position ? wheel.position.x : 0);
        var anchorFrame = (targetFlickable.contentX + mouseX - timeWidth) / oldScale; // timeWidth補正

        TimelineBridge.timelineScale = newScale;

        // ズーム後の位置補正
        var newContentX = anchorFrame * newScale - mouseX + timeWidth; 
        var maxX = Math.max(0, targetFlickable.contentWidth - targetFlickable.width);
        targetFlickable.contentX = clamp(newContentX, 0, maxX);
    }

    // Canvas再描画トリガー
    Connections {
        target: targetFlickable
        function onContentXChanged() { rulerCanvas.requestPaint(); }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // 左上の空白（レイヤーヘッダーの上）
        Rectangle {
            Layout.preferredWidth: timeWidth
            Layout.fillHeight: true
            color: palette.base
            border.color: palette.mid
            border.width: 1
            z: 100
            
            Text {
                anchors.centerIn: parent
                text: TimelineBridge && TimelineBridge.transport ? 
                      TimelineBridge.transport.currentFrame + "f" : "0f"
                font.pixelSize: 11
                font.bold: true
                color: palette.highlight
            }
        }

        // 定規本体
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Canvas {
                id: rulerCanvas
                anchors.fill: parent
                property double scale: TimelineBridge ? TimelineBridge.timelineScale : 1
                property double offsetX: targetFlickable ? targetFlickable.contentX : 0
                property int fpsInt: Math.round(rulerRoot.fps)

                onScaleChanged: requestPaint()
                onOffsetXChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    if (!TimelineBridge) return;

                    var viewWidth = width;
                    var viewOffsetX = offsetX;
                    
                    // 簡易的なグリッド描画（詳細は元のロジックを維持）
                    var frameInterval = 60; // 仮
                    if (scale > 5) frameInterval = 10;
                    else if (scale > 1) frameInterval = 30;
                    else if (scale > 0.5) frameInterval = 60;
                    else frameInterval = 300;

                    var startFrame = Math.floor(viewOffsetX / scale);
                    var endFrame = Math.ceil((viewOffsetX + viewWidth) / scale);
                    
                    var alignedStart = Math.floor(startFrame / frameInterval) * frameInterval;
                    
                    ctx.strokeStyle = palette.text;
                    ctx.fillStyle = palette.text;
                    ctx.lineWidth = 1;
                    ctx.font = "10px sans-serif";
                    
                    for (var f = alignedStart; f <= endFrame; f += frameInterval) {
                        var pixelX = f * scale - viewOffsetX;
                        
                        // 大目盛
                        ctx.beginPath();
                        ctx.moveTo(pixelX, 15);
                        ctx.lineTo(pixelX, height);
                        ctx.stroke();

                        // フレーム数テキスト
                        ctx.fillText(f, pixelX + 2, 12);
                    }
                }
            }

            // マウス操作（スクラブ & ズーム）
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                
                onPressed: (mouse) => {
                    if (mouse.button === Qt.LeftButton) {
                        if (TimelineBridge && TimelineBridge.transport && targetFlickable) {
                            TimelineBridge.transport.currentFrame = pxToFrame(mouse.x, targetFlickable.contentX);
                        }
                    }
                }
                
                onPositionChanged: (mouse) => {
                    if (pressed && mouse.buttons & Qt.LeftButton) {
                        if (TimelineBridge && TimelineBridge.transport && targetFlickable) {
                            TimelineBridge.transport.currentFrame = pxToFrame(mouse.x, targetFlickable.contentX);
                        }
                    }
                }

                onWheel: (wheel) => {
                    var dy = (wheel.angleDelta.y !== 0) ? wheel.angleDelta.y : (wheel.pixelDelta.y * 10);
                    var zoomFactor = (dy > 0) ? 1.1 : 0.9;
                    zoomAt(wheel, zoomFactor);
                    wheel.accepted = true;
                }
            }
        }
    }
}
