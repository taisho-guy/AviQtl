import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "common" as Common
import "timeline" // サブフォルダのモジュールをインポート

Common.RinaWindow {
    id: timelineWindow

    // 定数・設定
    property var settings: SettingsManager.settings
    readonly property int layerCount: settings.timelineMaxLayers || 128
    readonly property int layerHeight: settings.timelineTrackHeight || 30
    readonly property int rulerHeight: settings.timelineRulerHeight || 32
    readonly property int headerWidth: settings.timelineLayerHeaderWidth || 60
    readonly property int clipResizeHandleWidth: settings.timelineClipResizeHandleWidth || 10
    readonly property int sceneTabHeight: settings.timelineHeaderHeight || 28
    // レイヤー状態のグローバル管理（LayerHeaderからの通知を受け取る）
    property var globalLayerStates: ({
    })

    function getLayerVisible(layer) {
        var state = globalLayerStates[layer];
        return state ? state.visible : true;
    }

    title: "Timeline"
    width: 1280
    height: 300

    // グリッド設定ウィンドウ
    Loader {
        id: gridSettingsLoader

        active: false
        source: "timeline/GridSettingsWindow.qml"
        onLoaded: item.open(timelineView)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 1. シーンタブ
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: sceneTabHeight
            spacing: 0
            z: 1

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff

                TabBar {
                    id: sceneTabBar

                    // ScrollView内で適切に広がるように設定
                    width: Math.max(parent.width, contentWidth)

                    Repeater {
                        id: sceneRepeater

                        model: TimelineBridge ? TimelineBridge.scenes : []

                        TabButton {
                            id: tabBtn

                            checked: TimelineBridge && TimelineBridge.currentSceneId === modelData.id
                            onClicked: {
                                if (TimelineBridge)
                                    TimelineBridge.switchScene(modelData.id);

                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onClicked: {
                                    var win = WindowManager.getWindow("sceneSettings");
                                    if (win)
                                        win.openForScene(modelData.id, modelData.name, modelData.width !== undefined ? modelData.width : 1920, modelData.height !== undefined ? modelData.height : 1080, modelData.fps !== undefined ? modelData.fps : 60, modelData.totalFrames !== undefined ? modelData.totalFrames : 300);

                                }
                            }

                            contentItem: RowLayout {
                                spacing: 4

                                Text {
                                    text: modelData.name
                                    font: tabBtn.font
                                    color: palette.text
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    Layout.maximumWidth: 200
                                }

                                Button {
                                    flat: true
                                    visible: modelData.id !== 0
                                    Layout.preferredWidth: 20
                                    Layout.preferredHeight: 20
                                    onClicked: {
                                        if (TimelineBridge)
                                            TimelineBridge.removeScene(modelData.id);

                                    }

                                    contentItem: Common.RinaIcon {
                                        iconName: "close_line"
                                        size: 14
                                        color: parent.hovered ? parent.palette.highlight : parent.palette.text
                                    }

                                }

                            }

                        }

                    }

                }

            }

            // シーン追加ボタン
            Button {
                flat: true
                Layout.preferredWidth: 40
                Layout.fillHeight: true
                onClicked: TimelineBridge.createScene("Scene " + (sceneRepeater.count + 1))

                contentItem: Common.RinaIcon {
                    iconName: "add_line"
                    size: 20
                    color: parent.hovered ? parent.palette.highlight : parent.palette.text
                }

            }

        }

        // 2. 定規
        Ruler {
            targetFlickable: timelineView.flickable
            rulerHeight: timelineWindow.rulerHeight
            timeWidth: timelineWindow.headerWidth
            fps: TimelineBridge && TimelineBridge.project ? TimelineBridge.project.fps : 60
        }

        // 3. メインエリア
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            LayerHeader {
                id: layerHeader

                headerWidth: timelineWindow.headerWidth
                layerHeight: timelineWindow.layerHeight
                layerCount: timelineWindow.layerCount
                syncFlickable: timelineView.flickable
                onLayerVisibilityChanged: (layer, visible) => {
                    if (!globalLayerStates[layer])
                        globalLayerStates[layer] = {
                        "visible": true,
                        "locked": false
                    };

                    globalLayerStates[layer].visible = visible;
                    globalLayerStates = globalLayerStates; // 強制再評価
                }
                onLayerLockChanged: (layer, locked) => {
                    if (!globalLayerStates[layer])
                        globalLayerStates[layer] = {
                        "visible": true,
                        "locked": false
                    };

                    globalLayerStates[layer].locked = locked;
                    globalLayerStates = globalLayerStates;
                }
            }

            TimelineView {
                id: timelineView

                Layout.fillWidth: true
                Layout.fillHeight: true
                layerHeight: timelineWindow.layerHeight
                layerCount: timelineWindow.layerCount
                clipResizeHandleWidth: timelineWindow.clipResizeHandleWidth
                getLayerLocked: (layer) => {
                    return layerHeader.getLayerLocked(layer);
                }
                onGridSettingsRequested: {
                    if (!gridSettingsLoader.active)
                        gridSettingsLoader.active = true;
                    else
                        gridSettingsLoader.item.open(timelineView);
                }
            }

        }

    }

}
