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
    property double fps: TimelineBridge && TimelineBridge.project ? TimelineBridge.project.fps : 60.0
    property int selectedLayer: TimelineBridge ? TimelineBridge.selectedLayer : 0
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
                if (item && typeof item.destroy === 'function') {
                    try { item.destroy() } catch(e) { console.log("Failed to destroy menu item: " + e) }
                }
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

    // ========================================
    // タイムライン定規（AviUtl風）
    // ========================================
    Rectangle {
        id: rulerArea
        Layout.fillWidth: true
        Layout.preferredHeight: 32
        color: "#252525"
        z: 10
        
        property var targetFlickable: null
        
        Item {
            id: rulerContent
            anchors.fill: parent
            clip: true
            
            Canvas {
                id: rulerCanvas
                anchors.fill: parent
                
                property double scale: TimelineBridge ? TimelineBridge.timelineScale : 1.0
                property double offsetX: rulerArea.targetFlickable ? rulerArea.targetFlickable.contentX : 0
                property int fps: 30
                
                onScaleChanged: requestPaint()
                onOffsetXChanged: requestPaint()
                
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    
                    if (!TimelineBridge) return
                    
                    var scale = TimelineBridge.timelineScale
                    var viewWidth = width
                    var viewOffsetX = offsetX
                    
                    var minSpacing = 80
                    var frameInterval = 1
                    var intervals = [1, 5, 10, fps, fps*2, fps*5, fps*10, fps*20, fps*60, fps*300]
                    
                    for (var i = 0; i < intervals.length; i++) {
                        if (intervals[i] * scale >= minSpacing) {
                            frameInterval = intervals[i]
                            break
                        }
                    }
                    
                    var startFrame = Math.floor(viewOffsetX / scale)
                    var endFrame = Math.ceil((viewOffsetX + viewWidth) / scale)
                    var alignedStart = Math.floor(startFrame / frameInterval) * frameInterval
                    
                    ctx.font = "10px sans-serif"
                    
                    for (var f = alignedStart; f <= endFrame; f += frameInterval) {
                        if (f < 0) continue
                        
                        var pixelX = f * scale - viewOffsetX
                        var isSecond = (f % fps === 0)
                        
                        ctx.strokeStyle = isSecond ? "#ffffff" : "#666666"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(pixelX, height - (isSecond ? 12 : 6))
                        ctx.lineTo(pixelX, height)
                        ctx.stroke()
                        
                        if (isSecond) {
                            var totalSeconds = f / fps
                            var hours = Math.floor(totalSeconds / 3600)
                            var minutes = Math.floor((totalSeconds % 3600) / 60)
                            var seconds = Math.floor(totalSeconds % 60)
                            
                            var timeLabel = ""
                            if (hours > 0) {
                                timeLabel = hours + ":" + ("0" + minutes).slice(-2) + ":" + ("0" + seconds).slice(-2)
                            } else if (minutes > 0) {
                                timeLabel = minutes + ":" + ("0" + seconds).slice(-2)
                            } else {
                                timeLabel = seconds + "s"
                            }
                            
                            ctx.fillStyle = "#dddddd"
                            ctx.fillText(timeLabel, pixelX + 3, 12)
                            
                            ctx.font = "8px sans-serif"
                            ctx.fillStyle = "#888888"
                            ctx.fillText(f + "f", pixelX + 3, 24)
                            ctx.font = "10px sans-serif"
                        }
                    }
                }
            }
            
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                width: 70
                height: parent.height
                color: "#1a1a1a"
                border.color: "#444444"
                border.width: 1
                z: 100
                
                Text {
                    anchors.centerIn: parent
                    text: (TimelineBridge && TimelineBridge.transport) ? 
                          TimelineBridge.transport.currentFrame + "f" : "0f"
                    font.pixelSize: 11
                    font.bold: true
                    color: "#ff4444"
                }
            }
        }
        
        MouseArea {
            anchors.fill: parent
            anchors.leftMargin: 70
            
            onPressed: function(mouse) {
                if (TimelineBridge && TimelineBridge.transport && rulerArea.targetFlickable) {
                    var clickX = mouse.x + 70 + rulerArea.targetFlickable.contentX
                    var scale = TimelineBridge.timelineScale
                    var targetFrame = Math.max(0, Math.round(clickX / scale))
                    TimelineBridge.transport.currentFrame = targetFrame
                }
            }
            
            onPositionChanged: function(mouse) {
                if (pressed && TimelineBridge && TimelineBridge.transport && rulerArea.targetFlickable) {
                    var clickX = mouse.x + 70 + rulerArea.targetFlickable.contentX
                    var scale = TimelineBridge.timelineScale
                    var targetFrame = Math.max(0, Math.round(clickX / scale))
                    TimelineBridge.transport.currentFrame = targetFrame
                }
            }
            
            // ホイールでタイムライン縮尺を変更
            onWheel: function(wheel) {
                if (!TimelineBridge) return
                
                var currentScale = TimelineBridge.timelineScale
                var delta = wheel.angleDelta.y / 120.0
                var zoomFactor = 1.0 + (delta * 0.1)
                
                var newScale = currentScale * zoomFactor
                newScale = Math.max(0.1, Math.min(20.0, newScale))
                
                TimelineBridge.timelineScale = newScale
                wheel.accepted = true
            }
        }
        
        Component.onCompleted: {
            Qt.callLater(function() {
                if (typeof timelineFlickable !== "undefined") {
                    rulerArea.targetFlickable = timelineFlickable
                    rulerArea.targetFlickable.contentXChanged.connect(function() {
                        rulerCanvas.requestPaint()
                    })
                }
            })
        }
    }

    // メインエリア（左：レイヤーヘッダー、右：タイムライン）
    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 0

        // === 左側: レイヤーヘッダー (Layer 0-127) ===
        Rectangle {
            Layout.preferredWidth: 60
            Layout.fillHeight: true
            color: "#2a2a2a"
            z: 20 // タイムラインより手前に表示

            // 右側のタイムラインと縦スクロールを同期
            Flickable {
                id: layerHeaderFlickable
                anchors.fill: parent
                contentHeight: 128 * 30 // 128レイヤー * 30px
                contentY: timelineFlickable.contentY // 一方向バインディング（右→左）
                interactive: false // スクロール操作は右側に任せる
                clip: true

                Column {
                    Repeater {
                        model: 128 // Layer 0 to 127
                        
                        Rectangle {
                            width: 60
                            height: 30
                            color: index % 2 === 0 ? "#2a2a2a" : "#252525"
                            border.color: "#111"
                            border.width: 1
                            
                            property bool isHidden: false // 仮の状態管理
                            
                            Text {
                                anchors.centerIn: parent
                                text: "Layer " + (index + 1)
                                color: parent.isHidden ? "#555" : "#ddd"
                                font.pixelSize: 10
                            }
                            
                            // 状態表示アイコン（仮）
                            Rectangle {
                                width: 4
                                height: 4
                                radius: 2
                                color: parent.isHidden ? "transparent" : "#00ff00"
                                anchors.right: parent.right
                                anchors.rightMargin: 4
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    parent.isHidden = !parent.isHidden
                                }
                            }
                        }
                    }
                }
            }
            
            // 境界線
            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: "#444"
            }
        }

        // === 右側: タイムライン本体 ===
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            
            Flickable {
                id: timelineFlickable
                contentWidth: 2000
                contentHeight: 128 * 30
                interactive: true
                
                // contentYが変更されたらヘッダーも同期
                onContentYChanged: {
                    layerHeaderFlickable.contentY = contentY
                }

                // レイヤー区切り線
                Repeater {
                    model: 128
                    Rectangle {
                        y: index * 30
                        width: Math.max(parent.width, 2000)
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
                        
                        if (mouse.button === Qt.LeftButton) {
                            if (TimelineBridge) {
                                TimelineBridge.selectedLayer = layer
                            }
                        }
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
}