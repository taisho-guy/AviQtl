import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "common" as Common

Common.RinaWindow {
    id: timelineWindow
    width: 1280
    height: 300
    // x: 100; y: 500 // Waylandでは無視されることが多い
    title: "Timeline [Layer 1-100]"
    visible: true

    // コンテキストメニューの定義
    Menu {
        id: contextMenu

        // Context payload
        property string contextType: "timeline_background" // timeline_background | clip | layer_header | scene_header
        property int clickFrame: 0
        property int clickLayer: 0
        property int clickClipId: -1
        property var menuModel: []

        function openAt(x, y, ctxType, frame, layer, clipId) {
            contextType = ctxType
            clickFrame = frame
            clickLayer = layer
            clickClipId = clipId
            menuModel = buildMenuModel()
            popup(x, y)
        }

        function buildMenuModel() {
            // 最初はQML側で定義（後でC++に移してもよい）
            if (contextType === "clip") {
                return [
                    { type: "item", text: "Cut",     cmd: "clip.cut",     enabled: (clickClipId >= 0) },
                    { type: "item", text: "Copy",    cmd: "clip.copy",    enabled: (clickClipId >= 0) },
                    { type: "item", text: "Delete",  cmd: "clip.delete",  enabled: (clickClipId >= 0) },
                    { type: "sep" },
                    { type: "item", text: "Split",   cmd: "clip.split",   enabled: (clickClipId >= 0) },
                    { type: "item", text: "Group",   cmd: "clip.group",   enabled: false },
                    { type: "item", text: "Ungroup", cmd: "clip.ungroup", enabled: false }
                ]
            }

            // timeline_background
            return [
                { type: "item", text: "Add Text Object",  cmd: "add.text", enabled: true },
                { type: "item", text: "Add Rect Object",  cmd: "add.rect", enabled: true },
                { type: "sep" },
                { type: "item", text: "Paste",            cmd: "edit.paste", enabled: false },
                { type: "sep" },
                { type: "item", text: "Undo",             cmd: "edit.undo", enabled: true },
                { type: "item", text: "Redo",             cmd: "edit.redo", enabled: true }
            ]
        }

        Repeater {
            model: contextMenu.menuModel
            delegate: Loader {
                active: true
                // 重要: modelDataをLoaderのプロパティとして保存し、子からアクセス可能にする
                property var menuData: modelData
                sourceComponent: (modelData.type === "sep") ? sepComp : itemComp
            }
        }

        Component {
            id: sepComp
            MenuSeparator { }
        }
        Component {
            id: itemComp
            MenuItem {
                // Loader(parent)のプロパティ経由でデータにアクセス
                text: parent.menuData.text
                enabled: (parent.menuData.enabled !== undefined) ? parent.menuData.enabled : true
                onTriggered: {
                    if (!TimelineBridge) return
                    switch (parent.menuData.cmd) {
                    case "add.text":
                        TimelineBridge.createObject("text", contextMenu.clickFrame, contextMenu.clickLayer)
                        break
                    case "add.rect":
                        TimelineBridge.createObject("rect", contextMenu.clickFrame, contextMenu.clickLayer)
                        break
                    case "edit.undo":
                        TimelineBridge.undo()
                        break
                    case "edit.redo":
                        TimelineBridge.redo()
                        break
                    case "clip.delete":
                        // clickClipIdはContextMenuのプロパティなのでそのままアクセス可
                        TimelineBridge.deleteClip(contextMenu.clickClipId)
                        break
                    case "clip.split":
                        TimelineBridge.splitClip(contextMenu.clickClipId, contextMenu.clickFrame)
                        break
                    case "clip.cut":
                        TimelineBridge.cutClip(contextMenu.clickClipId)
                        break
                    case "clip.copy":
                        TimelineBridge.copyClip(contextMenu.clickClipId)
                        break
                    case "edit.paste":
                        TimelineBridge.pasteClip(contextMenu.clickFrame, contextMenu.clickLayer)
                        break
                    default:
                        TimelineBridge.log("Unknown cmd: " + parent.menuData.cmd)
                        break
                    }
                }
            }
        }
    }




    // 設定ウィンドウの参照を保持するプロパティ
    property var settingDialog: null

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ツールバー等は省略し、直接タイムラインエリアへ

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            
            Flickable {
                contentWidth: 2000
                contentHeight: 3000
                interactive: true

                // レイヤー区切り線
                Repeater {
                    model: 20
                    Rectangle {
                        y: index * 30
                        width: parent.width
                        height: 1
                        color: "#333"
                    }
                }

                // === Playhead (Current Time) ===
                Rectangle {
                    id: playhead
            property double scale: TimelineBridge ? TimelineBridge.timelineScale : 1.0
            
            x: (TimelineBridge && TimelineBridge.transport) ? (TimelineBridge.transport.currentFrame * scale) : 0
                    y: 0
                    width: 2
                    height: parent.height
                    color: "red"
                    z: 100 // 最前面

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -5 // 掴みやすくする
                        cursorShape: Qt.SizeHorCursor
                        drag.target: playhead
                        drag.axis: Drag.XAxis
                        drag.minimumX: 0
                        
                        onPositionChanged: {
                            if (drag.active && TimelineBridge && TimelineBridge.transport) {
                                TimelineBridge.transport.currentFrame = Math.round(playhead.x / scale) // ピクセル座標をフレーム数に変換
                            }
                        }
                    }
                }

                // 全体を覆うマウスエリア（オブジェクトがない場所のクリック用）
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton | Qt.LeftButton
                    z: -1 // オブジェクトより奥に配置

                    onClicked: (mouse) => {
                        if (mouse.button !== Qt.RightButton) return
                        var scale = TimelineBridge ? TimelineBridge.timelineScale : 1.0
                        var frame = Math.floor(mouse.x / scale)
                        var layer = Math.floor(mouse.y / 30) // 30px per layer
                        contextMenu.openAt(mouse.x, mouse.y, "timeline_background", frame, layer, -1)
                    }
                }

                // クリップ一覧表示 (Repeater)
                Repeater {
                    model: TimelineBridge ? TimelineBridge.clips : []
                    delegate: Rectangle {
                        id: clipRect
                        property double scale: TimelineBridge ? TimelineBridge.timelineScale : 1.0

                        // Layer 0 is at y=0. Each layer is 30px height.
                        y: modelData.layer * 30 
                        x: modelData.startFrame * scale
                        width: modelData.durationFrames * scale
                        height: 28
                        color: "#66aa99"
                        border.color: "white"
                        border.width: 1
                        opacity: 0.8
                        radius: 4
                        
                        Text {
                            anchors.centerIn: parent
                            text: modelData.type
                            color: "white"
                        }

                        // 右クリックは“クリップ上”として別系統で開く（背景Menuと混ざらない）
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            propagateComposedEvents: true
                            onPressed: (mouse) => {
                                if (!TimelineBridge) return
                                TimelineBridge.selectClip(modelData.id)
                            }
                            onClicked: (mouse) => {
                                var scale = TimelineBridge ? TimelineBridge.timelineScale : 1.0
                                // 相対座標ではなく、グローバル座標から計算するため mouse.x を使う
                                var frame = Math.floor(clipRect.x + mouse.x / scale)
                                contextMenu.openAt(mouse.x, mouse.y, "clip", frame, modelData.layer, modelData.id)
                            }
                        }

                        // === 移動用 MouseArea (本体) ===
                        MouseArea {
                            id: moveArea
                            anchors.fill: parent
                            anchors.rightMargin: 10 // リサイズ用エリアを空ける
                            
                            // ドラッグ設定
                            drag.target: clipRect
                            drag.axis: Drag.XAndYAxis
                            drag.minimumX: 0
                            drag.minimumY: 0
                            drag.smoothed: false

                            onPressed: {
                                if (TimelineBridge) TimelineBridge.selectClip(modelData.id)
                            }

                            onClicked: {
                                if (TimelineBridge) TimelineBridge.selectClip(modelData.id)
                            }

                            onDoubleClicked: {
                                if (WindowManager) WindowManager.raiseWindow("objectSettings")
                            }

                            onReleased: {
                                if (TimelineBridge) {
                                    var scale = TimelineBridge.timelineScale
                                    // 座標からフレーム/レイヤーを計算
                                    var newStart = Math.round(clipRect.x / scale)
                                    var newLayer = Math.round(clipRect.y / 30)
                                    
                                    // バインディングを復元しつつ更新
                                    TimelineBridge.updateClip(modelData.id, newLayer, newStart, modelData.durationFrames)
                                }
                            }
                        }

                        // === リサイズ用ハンドル (右端) ===
                        Rectangle {
                            width: 10
                            height: parent.height
                            anchors.right: parent.right
                            color: "transparent"
                            // 視覚的フィードバック（ホバー時に少し白くする等しても良い）

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.SizeHorCursor
                                hoverEnabled: true // カーソル変更のために必要
                                preventStealing: true // ドラッグを確実にする
                                
                                property int startX: 0
                                property int startWidth: 0
                                property bool resizing: false

                                onPressed: {
                                    startX = mouseX
                                    startWidth = clipRect.width
                                    resizing = true
                                    if (TimelineBridge) TimelineBridge.selectClip(modelData.id)
                                }

                                onPositionChanged: {
                                    if (resizing) {
                                        // 単純な差分加算
                                        var delta = mouseX - startX
                                        var newW = startWidth + delta
                                        if (newW < 5) newW = 5
                                        clipRect.width = newW
                                        // startWidth等は更新しない（累積誤差を防ぐため初期位置基準）
                                    }
                                }

                                onReleased: {
                                    resizing = false
                                    if (TimelineBridge) {
                                        var scale = TimelineBridge.timelineScale
                                        var newDur = Math.round(clipRect.width / scale)
                                        TimelineBridge.updateClip(modelData.id, modelData.layer, modelData.startFrame, newDur)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}