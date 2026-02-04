import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "common" as Common
import "timeline" // サブフォルダのモジュールをインポート

Common.RinaWindow {
    id: timelineWindow
    title: "Timeline"
    width: 1280
    height: 300

    // 定数・設定
    property var settings: SettingsManager.settings
    readonly property int layerCount: settings.timelineMaxLayers || 128
    readonly property int layerHeight: settings.timelineTrackHeight || 30
    readonly property int rulerHeight: settings.timelineRulerHeight || 32
    readonly property int headerWidth: settings.timelineLayerHeaderWidth || 60
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 1. シーンタブ (簡易)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: palette.window
            border.color: palette.mid
            border.width: 0 // 下線はLayoutで調整
            
            ListView {
                anchors.fill: parent
                orientation: ListView.Horizontal
                model: TimelineBridge ? TimelineBridge.scenes : []
                delegate: Button {
                    text: modelData.name
                    flat: true
                    highlighted: TimelineBridge.currentSceneId === modelData.id
                    onClicked: TimelineBridge.switchScene(modelData.id)
                }
            }
        }

        // 2. 定規エリア
        Ruler {
            targetFlickable: timelineView.flickable
            rulerHeight: timelineWindow.rulerHeight
            timeWidth: timelineWindow.headerWidth
            fps: TimelineBridge && TimelineBridge.project ? TimelineBridge.project.fps : 60
        }

        // 3. メインエリア (左：ヘッダー、右：タイムライン)
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            LayerHeader {
                headerWidth: timelineWindow.headerWidth
                layerHeight: timelineWindow.layerHeight
                layerCount: timelineWindow.layerCount
                syncFlickable: timelineView.flickable
            }

            TimelineView {
                id: timelineView
                Layout.fillWidth: true
                Layout.fillHeight: true
                
                layerHeight: timelineWindow.layerHeight
                layerCount: timelineWindow.layerCount
            }
        }
    }
}
