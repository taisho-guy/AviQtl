import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: headerRoot
    
    property int headerWidth: 60
    property int layerHeight: 30
    property int layerCount: 128
    property var syncFlickable: null // TimelineViewのFlickable

    Layout.preferredWidth: headerWidth
    Layout.fillHeight: true
    color: palette.button
    z: 2

    function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }

    Flickable {
        id: layerHeaderFlickable
        anchors.fill: parent
        contentHeight: layerCount * layerHeight
        // TimelineViewと同期
        contentY: syncFlickable ? syncFlickable.contentY : 0
        interactive: false // 独自のホイール処理を行うため
        clip: true

        Column {
            Repeater {
                model: layerCount
                Rectangle {
                    width: headerRoot.headerWidth
                    height: headerRoot.layerHeight
                    color: (index % 2 == 0) ? palette.window : Qt.darker(palette.window, 1.05)
                    border.color: palette.mid
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Layer " + (index + 1)
                        color: palette.text
                        font.pixelSize: 10
                    }
                }
            }
        }
    }

    // 縦スクロール専用マウスエリア
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        hoverEnabled: true
        onWheel: (wheel) => {
            if (!syncFlickable) return;
            
            var dy = wheel.angleDelta.y;
            if (wheel.pixelDelta && wheel.pixelDelta.y !== 0) dy = wheel.pixelDelta.y * 10;
            
            var nextY = syncFlickable.contentY - dy;
            var maxY = Math.max(0, syncFlickable.contentHeight - syncFlickable.height);
            syncFlickable.contentY = clamp(nextY, 0, maxY);
            wheel.accepted = true;
        }
    }
}
