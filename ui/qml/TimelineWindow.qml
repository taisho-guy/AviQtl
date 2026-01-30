import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml 2.15
import "common" as Common

Common.RinaWindow {
    id: timelineWindow
    width: 1280
    height: 300
    // x: 100; y: 500 // Waylandでは無視されることが多い
    title: "タイムライン [レイヤー 1-100]"
    visible: true

    // コンテキストメニューの定義
    Menu {
        id: contextMenu

        // Context payload
        property string contextType: "timeline_background"
        property int clickFrame: 0
        property int clickLayer: 0
        property int clickClipId: -1

        function openAt(x, y, ctxType, frame, layer, clipId) {
            contextType = ctxType
            clickFrame = frame
            clickLayer = layer
            clickClipId = clipId
            rebuildMenu()
            popup(x, y)
        }

        function rebuildMenu() {
            // 既存の項目をすべてクリア
            while (contextMenu.count > 0) {
                var item = contextMenu.itemAt(0)
                contextMenu.removeItem(item)
                item.destroy()
            }
            
            // コンテキストに応じて項目を追加
            if (contextType === "clip") {
                contextMenu.addItem(createMenuItem("コピー", "clip.copy"))
                contextMenu.addItem(createMenuItem("貼り付け", "edit.paste"))
                contextMenu.addItem(createMenuItem("切り取り", "clip.cut"))
                contextMenu.addItem(createMenuItem("分割", "clip.split"))
                addSeparator()
                contextMenu.addItem(createMenuItem("削除", "clip.delete"))
                addSeparator()
                contextMenu.addItem(createMenuItem("グループ化", "clip.group", false))
                contextMenu.addItem(createMenuItem("グループ化解除", "clip.ungroup", false))
                addSeparator()
                contextMenu.addItem(createMenuItem("上のオブジェクトでクリッピング", "clip.clip_above", false))
                contextMenu.addItem(createMenuItem("下のオブジェクトをクリッピング", "clip.clip_below", false))
                addSeparator()
                var cameraItem = createMenuItem("カメラ制御の対象", "clip.camera_target")
                cameraItem.checkable = true
                cameraItem.checked = false
                contextMenu.addItem(cameraItem)
            } else {
                // timeline_background
                var mediaMenu = createSubMenu("メディアオブジェクトを追加")
                
                // 動的生成: category="object"
                if (TimelineBridge) {
                    var objects = TimelineBridge.getAvailableObjects("object")
                    for (var i = 0; i < objects.length; i++) {
                        var obj = objects[i]
                        mediaMenu.addItem(createMenuItem(obj.name, "add." + obj.id))
                    }
                }
                
                // フォールバック
                if (mediaMenu.count === 0) {
                    mediaMenu.addItem(createMenuItem("テキスト", "add.text"))
                    mediaMenu.addItem(createMenuItem("図形", "add.rect"))
                }
                contextMenu.addMenu(mediaMenu)
                
                var effectMenu = createSubMenu("フィルタオブジェクトを追加")
                // 現状はハードコードのまま維持（必要に応じて動的化）
                effectMenu.addItem(createMenuItem("シーンチェンジ", "add.scene_change", false))
                effectMenu.addItem(createMenuItem("カメラ制御", "add.camera_control", false))
                contextMenu.addMenu(effectMenu)
                
                addSeparator()
                contextMenu.addItem(createMenuItem("貼り付け", "edit.paste"))
                contextMenu.addItem(createMenuItem("元に戻す", "edit.undo"))
                addSeparator()
                
                var bpmGrid = createMenuItem("BPMグリッドを表示", "view.bpm_grid")
                bpmGrid.checkable = true
                bpmGrid.checked = false
                contextMenu.addItem(bpmGrid)
                
                var xyGrid = createMenuItem("XY軸グリッドを表示", "view.xy_grid")
                xyGrid.checkable = true
                xyGrid.checked = false
                contextMenu.addItem(xyGrid)
                
                var camGrid = createMenuItem("カメラ制御グリッドを表示", "view.cam_grid")
                camGrid.checkable = true
                camGrid.checked = false
                contextMenu.addItem(camGrid)
                
                contextMenu.addItem(createMenuItem("グリッドの設定", "view.grid_settings"))
            }
        }

        function createMenuItem(label, command, enabledFlag) {
            var enabled = (enabledFlag !== undefined) ? enabledFlag : true
            var item = menuItemComp.createObject(null, {
                text: label,
                enabled: enabled
            })
            if (command) {
                item.triggered.connect(function() {
                    handleCommand(command)
                })
            }
            return item
        }

        function createSubMenu(label) {
            var menu = subMenuComp.createObject(null, {
                title: label
            })
            return menu
        }

        function addSeparator() {
            var sep = menuSeparatorComp.createObject(null)
            contextMenu.addItem(sep)
        }

        function handleCommand(cmd) {
            if (!TimelineBridge) return
            
            // 動的コマンドの処理 "add.xxx"
            if (cmd.startsWith("add.")) {
                var type = cmd.substring(4)
                TimelineBridge.createObject(type, clickFrame, clickLayer)
                return
            }

            switch (cmd) {
                case "edit.undo":
                    TimelineBridge.undo()
                    break
                case "edit.redo":
                    TimelineBridge.redo()
                    break
                case "clip.delete":
                    TimelineBridge.deleteClip(clickClipId)
                    break
                case "clip.split":
                    TimelineBridge.splitClip(clickClipId, clickFrame)
                    break
                case "clip.cut":
                    TimelineBridge.cutClip(clickClipId)
                    break
                case "clip.copy":
                    TimelineBridge.copyClip(clickClipId)
                    break
                case "edit.paste":
                    TimelineBridge.pasteClip(clickFrame, clickLayer)
                    break
                default:
                    console.log("Unknown cmd:", cmd)
                    break
            }
        }

        Component {
            id: menuItemComp
            MenuItem {}
        }

        Component {
            id: subMenuComp
            Menu {}
        }

        Component {
            id: menuSeparatorComp
            MenuSeparator {}
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