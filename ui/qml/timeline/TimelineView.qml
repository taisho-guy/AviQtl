import "../common" as Common
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: timelineViewRoot

    property alias flickable: timelineFlickable
    property alias contentX: timelineFlickable.contentX
    property alias contentY: timelineFlickable.contentY
    property int layerHeight: 30
    property int layerCount: 128
    property int clipResizeHandleWidth
    property var getLayerLocked: function(layer) { return false }
    property int contextClickFrame: 0
    property int contextClickLayer: 0
    property bool enableSnap: SettingsManager.settings.enableSnap
    property int tailPaddingFrames: 120
    property var gridSettings: ({
        "mode": "Auto", "bpm": 120, "offset": 0, "interval": 10, "subdivision": 4
    })

    readonly property int maxClipEndFrame: {
        var maxEnd = 0
        var clipList = (TimelineBridge && TimelineBridge.clips) ? TimelineBridge.clips : []
        for (var i = 0; i < clipList.length; i++) {
            var end = clipList[i].startFrame + clipList[i].durationFrames
            if (end > maxEnd) maxEnd = end
        }
        return maxEnd
    }
    readonly property int timelineLengthFrames: Math.max(100, maxClipEndFrame + tailPaddingFrames)

    signal gridSettingsRequested()

    function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }

    function getGridInterval() {
        if (!TimelineBridge) return 1
        var scale = TimelineBridge.timelineScale
        var projectFps = (TimelineBridge.project && TimelineBridge.project.fps) ? TimelineBridge.project.fps : 60
        if (gridSettings.mode === "BPM") {
            var beatFrames = projectFps / (gridSettings.bpm / 60)
            var bpmDiv = scale > 3 ? 4 : scale > 1.5 ? 2 : 1
            return beatFrames / bpmDiv
        }
        if (gridSettings.mode === "Frame") return gridSettings.interval
        // Auto
        if (scale < 0.5)  return Math.ceil(projectFps)
        if (scale < 1.5)  return 10
        if (scale < 3)    return 5
        return 1
    }

    function snapFrame(frame) {
        if (!enableSnap) return Math.round(frame)
        var step   = getGridInterval()
        var offset = (gridSettings.mode === "BPM" && TimelineBridge && TimelineBridge.project)
                     ? gridSettings.offset * TimelineBridge.project.fps : 0
        return Math.max(0, Math.round((Math.round((frame - offset) / step) * step) + offset))
    }

    onGridSettingsChanged: timelineGrid.requestPaint()
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOn
    ScrollBar.vertical.policy:   ScrollBar.AlwaysOn

    // =========================================================
    // Flickable
    // =========================================================
    Flickable {
        id: timelineFlickable

        contentWidth:  Math.max(width, timelineLengthFrames * (TimelineBridge ? TimelineBridge.timelineScale : 1))
        contentHeight: layerCount * layerHeight
        interactive:   true

        Connections {
            target: timelineFlickable
            function onContentXChanged() { timelineGrid.requestPaint() }
            function onContentYChanged() { timelineGrid.requestPaint() }
        }
        Connections {
            target: TimelineBridge ?? null
            function onTimelineScaleChanged() { timelineGrid.requestPaint() }
        }

        // ── グリッド背景 ──────────────────────────────────────
        Item {
            x: timelineFlickable.contentX
            y: timelineFlickable.contentY
            width:  timelineFlickable.width
            height: timelineFlickable.height
            z: -1

            Canvas {
                id: timelineGrid
                anchors.fill: parent

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.lineWidth = 1

                    // 水平区切り線
                    ctx.strokeStyle = Qt.rgba(0.5, 0.5, 0.5, 0.2)
                    var startY = timelineFlickable.contentY
                    for (var i = 0; i < layerCount; i++) {
                        var ly = i * layerHeight - startY
                        if (ly < -layerHeight || ly > height) continue
                        ctx.beginPath(); ctx.moveTo(0, ly); ctx.lineTo(width, ly); ctx.stroke()
                    }

                    if (!TimelineBridge) return

                    // 垂直グリッド線
                    var scale    = TimelineBridge.timelineScale
                    var contentX = timelineFlickable.contentX
                    var step     = timelineViewRoot.getGridInterval()
                    var offsetF  = (gridSettings.mode === "BPM" && TimelineBridge.project)
                                   ? gridSettings.offset * TimelineBridge.project.fps : 0
                    var isBpm    = (gridSettings.mode === "BPM")
                    var bpmDiv   = isBpm ? (scale > 3 ? 4 : scale > 1.5 ? 2 : 1) : 1
                    var startN   = Math.ceil((Math.floor(contentX / scale) - offsetF) / step)
                    var endN     = Math.floor((Math.ceil((contentX + width) / scale) - offsetF) / step)

                    for (var n = startN; n <= endN; n++) {
                        var f = offsetF + n * step
                        var x = f * scale - contentX
                        ctx.beginPath()
                        if (isBpm) {
                            var isMeasure = (n % (gridSettings.subdivision * bpmDiv) === 0)
                            var isBeat    = (n % bpmDiv === 0)
                            if (isMeasure)      { ctx.strokeStyle = Qt.rgba(0.5, 0.8, 1, 0.5); ctx.lineWidth = 1.5 }
                            else if (isBeat)    { ctx.strokeStyle = Qt.rgba(0.5, 0.5, 0.5, 0.3); ctx.lineWidth = 1 }
                            else                { ctx.strokeStyle = Qt.rgba(0.5, 0.5, 0.5, 0.15); ctx.lineWidth = 1 }
                        } else {
                            ctx.strokeStyle = Qt.rgba(0.5, 0.5, 0.5, 0.15); ctx.lineWidth = 1
                        }
                        ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke()
                    }
                }
            }
        }

        // ── グループ制御カーテン ──────────────────────────────
        Item {
            id: groupControlCurtain

            property int selectedClipId: (TimelineBridge && TimelineBridge.selection)
                                          ? TimelineBridge.selection.selectedClipId : -1
            property var targetEffect: null
            property int layerCount:   0
            property var clipData:     null

            function update() {
                targetEffect = null; clipData = null; layerCount = 0
                if (selectedClipId < 0 || !TimelineBridge) return
                var clips = TimelineBridge.clips
                for (var i = 0; i < clips.length; i++) {
                    if (clips[i].id === selectedClipId) { clipData = clips[i]; break }
                }
                if (!clipData) return
                var effects = TimelineBridge.getClipEffectsModel(selectedClipId)
                for (var j = 0; j < effects.length; j++) {
                    if (effects[j].id === "GroupControl") {
                        targetEffect = effects[j]
                        layerCount   = targetEffect.params["layerCount"] || 0
                        break
                    }
                }
            }

            z:       -0.5
            visible: targetEffect !== null && layerCount > 0
            x:       clipData ? clipData.startFrame * (TimelineBridge ? TimelineBridge.timelineScale : 1) : 0
            y:       clipData ? (clipData.layer + 1) * timelineViewRoot.layerHeight : 0
            width:   clipData ? clipData.durationFrames * (TimelineBridge ? TimelineBridge.timelineScale : 1) : 0
            height:  layerCount * timelineViewRoot.layerHeight

            Connections {
                target: TimelineBridge?.selection ?? null
                function onSelectedClipIdChanged() { groupControlCurtain.update() }
            }
            Connections {
                target: TimelineBridge
                function onClipsChanged()              { groupControlCurtain.update() }
                function onClipEffectsChanged(clipId)  {
                    if (clipId === groupControlCurtain.selectedClipId) groupControlCurtain.update()
                }
            }
            Connections {
                target: groupControlCurtain.targetEffect
                ignoreUnknownSignals: true
                function onParamsChanged() {
                    if (groupControlCurtain.targetEffect)
                        groupControlCurtain.layerCount = groupControlCurtain.targetEffect.params["layerCount"] || 0
                }
            }

            Rectangle {
                anchors.fill: parent
                color:        Qt.rgba(0, 0.8, 0, 0.15)
                border.color: Qt.rgba(0, 0.8, 0, 0.4)
                border.width: 1
            }
        }

        // =========================================================
        // クリップ Repeater
        // =========================================================
        Repeater {
            model: TimelineBridge ? TimelineBridge.clips : []

            delegate: Item {
                id: clipDelegate

                // ── ドラフト状態（リサイズ中の仮値） ─────────────
                // -1 = ドラフトなし（モデル値で表示）
                property int  resizeDraftStart:    -1
                property int  resizeDraftDuration: -1

                // ── 表示位置・サイズ (バインディングは常に生きている) ──
                property double scale: TimelineBridge ? TimelineBridge.timelineScale : 1

                x:      (resizeDraftStart    >= 0 ? resizeDraftStart    : modelData.startFrame)    * scale
                y:      modelData.layer * layerHeight
                width:  (resizeDraftDuration >= 0 ? resizeDraftDuration : modelData.durationFrames) * scale
                height: layerHeight
                z:      modelData.layer

                readonly property bool isSelected:    TimelineBridge && TimelineBridge.selection
                                                      && TimelineBridge.selection.selectedClipId === modelData.id
                readonly property bool isLayerLocked: getLayerLocked(modelData.layer)
                property int  groupLayerCount:  0
                property var  groupEffectModel: null
                property string clipDisplayName: modelData.type

                function updateGroupInfo() {
                    groupLayerCount = 0; groupEffectModel = null
                    if (!isSelected || !TimelineBridge) return
                    var effects = TimelineBridge.getClipEffectsModel(modelData.id)
                    for (var i = 0; i < effects.length; i++) {
                        if (effects[i].id === "GroupControl") {
                            groupEffectModel = effects[i]
                            groupLayerCount  = groupEffectModel.params["layerCount"] || 0
                            break
                        }
                    }
                }

                onIsSelectedChanged:    updateGroupInfo()
                Component.onCompleted:  updateGroupInfo()

                Connections {
                    target: TimelineBridge
                    function onClipEffectsChanged(clipId) {
                        if (clipId === modelData.id) clipDelegate.updateGroupInfo()
                    }
                }
                Connections {
                    target: clipDelegate.groupEffectModel
                    ignoreUnknownSignals: true
                    function onParamsChanged() {
                        if (clipDelegate.groupEffectModel)
                            clipDelegate.groupLayerCount = clipDelegate.groupEffectModel.params["layerCount"] || 0
                    }
                }

                // グループ制御範囲ハイライト
                Rectangle {
                    visible:      clipDelegate.isSelected && clipDelegate.groupLayerCount > 0
                    x: 0; y: layerHeight
                    width:        parent.width
                    height:       clipDelegate.groupLayerCount * layerHeight
                    color:        Qt.rgba(0, 0.8, 0, 0.15)
                    border.color: Qt.rgba(0, 0.8, 0, 0.4)
                    border.width: 1
                    z: -1
                }

                // ── クリップ本体 ───────────────────────────────
                Rectangle {
                    id: clipRect
                    anchors.fill:         parent
                    anchors.bottomMargin: 2
                    color:        isSelected ? "#77ccbb" : "#66aa99"
                    border.color: isSelected ? "#ff8800" : "#ffffff"
                    border.width: isSelected ? 2 : 1
                    opacity: 0.8
                    radius:  4

                    Text {
                        readonly property int    padding: 4
                        property real stickyX: Math.max(0, timelineFlickable.contentX - clipDelegate.x)
                        anchors.verticalCenter: parent.verticalCenter
                        text:  clipDelegate.clipDisplayName + " (" + modelData.id + ")"
                        color: "white"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                        x:     Math.min(stickyX, parent.width - width - padding) + padding
                        width: Math.min(implicitWidth, parent.width - padding * 2)
                    }

                    // ── 移動 MouseArea ────────────────────────
                    MouseArea {
                        id: moveArea

                        property point dragStartScenePos: Qt.point(0, 0)
                        property int   initialLayer:       0
                        property int   initialFrame:       0
                        property int   lastProposedFrame: -1
                        property int   lastProposedLayer: -1

                        anchors.fill:        parent
                        anchors.leftMargin:  clipResizeHandleWidth
                        anchors.rightMargin: clipResizeHandleWidth
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape:     clipDelegate.isLayerLocked ? Qt.ForbiddenCursor : Qt.OpenHandCursor
                        preventStealing: true

                        onPressed: (mouse) => {
                            if (clipDelegate.isLayerLocked) return
                            if (TimelineBridge) TimelineBridge.selectClip(modelData.id)
                            var sp = mapToItem(timelineFlickable.contentItem, mouse.x, mouse.y)
                            dragStartScenePos = sp
                            initialLayer      = modelData.layer
                            initialFrame      = modelData.startFrame
                            lastProposedFrame = -1
                            lastProposedLayer = -1
                        }
                        onPositionChanged: (mouse) => {
                            if (!pressed || clipDelegate.isLayerLocked) return
                            var sp      = mapToItem(timelineFlickable.contentItem, mouse.x, mouse.y)
                            var dX      = sp.x - dragStartScenePos.x
                            var dY      = sp.y - dragStartScenePos.y
                            var rawF    = initialFrame + dX / clipDelegate.scale
                            var propF   = timelineViewRoot.snapFrame(rawF)
                            var propL   = Math.max(0, Math.min(
                                              initialLayer + Math.round(dY / layerHeight),
                                              timelineViewRoot.layerCount - 1))
                            propF = Math.max(0, propF)
                            if (propF === lastProposedFrame && propL === lastProposedLayer) return
                            lastProposedFrame = propF; lastProposedLayer = propL

                            var finalF = propF, finalL = propL
                            if (TimelineBridge && typeof TimelineBridge.resolveDragPosition === "function") {
                                var pos = TimelineBridge.resolveDragPosition(modelData.id, propL, propF)
                                finalF = pos.x; finalL = pos.y
                            }
                            if (finalF !== propF) {
                                timelineFlickable.contentX = Math.max(
                                    0, timelineFlickable.contentX + (finalF - propF) * clipDelegate.scale)
                            }
                            clipDelegate.x = finalF * clipDelegate.scale
                            clipDelegate.y = finalL * layerHeight
                        }
                        onReleased: {
                            if (!TimelineBridge) return
                            var newStart = Math.round(clipDelegate.x / clipDelegate.scale)
                            var newLayer = Math.round(clipDelegate.y / layerHeight)
                            TimelineBridge.updateClip(modelData.id, newLayer, newStart, modelData.durationFrames)
                            // バインディング復元 (ドラフトは使っていないので x/y のみ)
                            clipDelegate.x = Qt.binding(() => (clipDelegate.resizeDraftStart >= 0
                                ? clipDelegate.resizeDraftStart : modelData.startFrame) * clipDelegate.scale)
                            clipDelegate.y = Qt.binding(() => modelData.layer * layerHeight)
                        }
                        onDoubleClicked: { if (WindowManager) WindowManager.raiseWindow("objectSettings") }
                        onClicked: (mouse) => {
                            if (mouse.button === Qt.RightButton) {
                                var frame = timelineViewRoot.snapFrame(
                                    (clipDelegate.x + mouse.x) / clipDelegate.scale)
                                contextMenu.openAt(mouse.x, mouse.y, "clip", frame, modelData.layer, modelData.id)
                            }
                        }
                    }

                    // ═══════════════════════════════════════════
                    // 左端リサイズハンドル
                    // 修正: mapToItem(contentItem) で絶対座標を使用
                    //       → mouseX のローカル座標ズレを完全排除
                    // ═══════════════════════════════════════════
                    Rectangle {
                        width:        clipResizeHandleWidth
                        height:       parent.height
                        anchors.left: parent.left
                        color: "transparent"
                        z: 10

                        MouseArea {
                            id: leftResizeArea

                            property real startSceneX:  0   // Flickable contentItem 座標
                            property int  startFrame:   0
                            property int  startDuration:0
                            property bool resizing:     false

                            anchors.fill:    parent
                            cursorShape:     Qt.SizeHorCursor
                            hoverEnabled:    true
                            preventStealing: true

                            onPressed: (mouse) => {
                                if (clipDelegate.isLayerLocked) return
                                var sp      = mapToItem(timelineFlickable.contentItem, mouse.x, mouse.y)
                                startSceneX  = sp.x
                                startFrame   = modelData.startFrame
                                startDuration = modelData.durationFrames
                                resizing     = true
                                if (TimelineBridge) TimelineBridge.selectClip(modelData.id)
                                mouse.accepted = true
                            }
                            onPositionChanged: (mouse) => {
                                if (!resizing) return
                                var sp      = mapToItem(timelineFlickable.contentItem, mouse.x, mouse.y)
                                var delta   = sp.x - startSceneX
                                // 右端（終点）を固定して左端のみ動かす
                                var endFrame   = startFrame + startDuration
                                var rawNewStart = startFrame + delta / clipDelegate.scale
                                var newStart   = Math.max(0, timelineViewRoot.snapFrame(rawNewStart))
                                var newDur     = endFrame - newStart
                                if (newDur < 5) {
                                    newStart = endFrame - 5
                                    newDur   = 5
                                }
                                // ドラフトプロパティ経由で表示更新（バインディング破壊なし）
                                clipDelegate.resizeDraftStart    = newStart
                                clipDelegate.resizeDraftDuration = newDur
                            }
                            onReleased: {
                                if (!resizing) return
                                resizing = false
                                if (TimelineBridge && clipDelegate.resizeDraftDuration > 0) {
                                    TimelineBridge.updateClip(
                                        modelData.id,
                                        modelData.layer,
                                        clipDelegate.resizeDraftStart >= 0
                                            ? clipDelegate.resizeDraftStart
                                            : modelData.startFrame,
                                        clipDelegate.resizeDraftDuration)
                                }
                                // ドラフト解除 → バインディングが自動で正値を返す
                                clipDelegate.resizeDraftStart    = -1
                                clipDelegate.resizeDraftDuration = -1
                            }
                        }
                    }

                    // ═══════════════════════════════════════════
                    // 右端リサイズハンドル
                    // 修正: mapToItem(contentItem) で絶対座標を使用
                    //       → ハンドル自体が動くことによる mouseX のドリフトを完全排除
                    // ═══════════════════════════════════════════
                    Rectangle {
                        width:         clipResizeHandleWidth
                        height:        parent.height
                        anchors.right: parent.right
                        color: "transparent"
                        z: 10

                        MouseArea {
                            id: rightResizeArea

                            property real startSceneX:  0   // Flickable contentItem 座標
                            property int  startFrame:   0   // リサイズ開始時の startFrame
                            property int  startDuration:0
                            property bool resizing:     false

                            anchors.fill:    parent
                            cursorShape:     Qt.SizeHorCursor
                            hoverEnabled:    true
                            preventStealing: true

                            onPressed: (mouse) => {
                                if (clipDelegate.isLayerLocked) return
                                var sp       = mapToItem(timelineFlickable.contentItem, mouse.x, mouse.y)
                                startSceneX   = sp.x
                                startFrame    = modelData.startFrame
                                startDuration = modelData.durationFrames
                                resizing      = true
                                if (TimelineBridge) TimelineBridge.selectClip(modelData.id)
                                mouse.accepted = true
                            }
                            onPositionChanged: (mouse) => {
                                if (!resizing) return
                                var sp      = mapToItem(timelineFlickable.contentItem, mouse.x, mouse.y)
                                var delta   = sp.x - startSceneX
                                // 右端フレームをスナップ
                                var rawEndFrame     = startFrame + (startDuration * clipDelegate.scale + delta) / clipDelegate.scale
                                var snappedEndFrame = timelineViewRoot.snapFrame(rawEndFrame)
                                var newDur          = Math.max(5, snappedEndFrame - startFrame)
                                // ドラフトプロパティ経由で表示更新（バインディング破壊なし）
                                clipDelegate.resizeDraftDuration = newDur
                            }
                            onReleased: {
                                if (!resizing) return
                                resizing = false
                                if (TimelineBridge && clipDelegate.resizeDraftDuration > 0) {
                                    TimelineBridge.updateClip(
                                        modelData.id,
                                        modelData.layer,
                                        modelData.startFrame,
                                        clipDelegate.resizeDraftDuration)
                                }
                                // ドラフト解除 → バインディングが自動で正値を返す
                                clipDelegate.resizeDraftDuration = -1
                            }
                        }
                    }
                }
            }
        }

        // ── 再生ヘッド ────────────────────────────────────────
        Rectangle {
            id: playhead
            x:      (TimelineBridge && TimelineBridge.transport ? TimelineBridge.transport.currentFrame : 0)
                    * (TimelineBridge ? TimelineBridge.timelineScale : 1)
            y:      0
            width:  2
            height: parent.height
            color:  "red"
            z:      100
        }

        // ── 背景クリック（再生ヘッド移動・コンテキストメニュー） ──
        MouseArea {
            anchors.fill:    parent
            z:               -1
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (mouse) => {
                var scale = TimelineBridge ? TimelineBridge.timelineScale : 1
                var frame = timelineViewRoot.snapFrame(mouse.x / scale)
                if (mouse.button === Qt.LeftButton) {
                    if (TimelineBridge && TimelineBridge.transport)
                        TimelineBridge.transport.currentFrame = frame
                } else {
                    var layer = Math.floor(mouse.y / layerHeight)
                    contextMenu.openAt(mouse.x, mouse.y, "timeline", frame, layer, -1)
                }
            }
        }

        // ── ホイール（スクロール／ズーム） ────────────────────
        MouseArea {
            anchors.fill:    parent
            acceptedButtons: Qt.NoButton
            onWheel: (wheel) => {
                var dy = (wheel.pixelDelta && wheel.pixelDelta.y !== 0) ? wheel.pixelDelta.y * 10 : wheel.angleDelta.y
                var dx = (wheel.pixelDelta && wheel.pixelDelta.x !== 0) ? wheel.pixelDelta.x * 10 : wheel.angleDelta.x
                if (wheel.modifiers & Qt.ShiftModifier) {
                    var maxY = Math.max(0, timelineFlickable.contentHeight - timelineFlickable.height)
                    timelineFlickable.contentY = clamp(timelineFlickable.contentY - dy, 0, maxY)
                } else {
                    var delta = (Math.abs(dx) > Math.abs(dy)) ? dx : dy
                    var maxX  = Math.max(0, timelineFlickable.contentWidth - timelineFlickable.width)
                    timelineFlickable.contentX = clamp(timelineFlickable.contentX - delta, 0, maxX)
                }
                wheel.accepted = true
            }
        }
    }

    // =========================================================
    // コンテキストメニュー
    // =========================================================
    Menu {
        id: contextMenu

        property string targetType:   ""
        property int    targetClipId: -1

        function openAt(x, y, type, frame, layer, clipId) {
            targetType    = type
            targetClipId  = clipId
            contextClickFrame = frame
            contextClickLayer = layer
            rebuildMenu()
            popup()
        }

        function createMenuItem(label, cmd, icon) {
            var item = menuItemComp.createObject(null, { "text": label, "iconName": icon || "" })
            item.triggered.connect(() => handleCommand(cmd))
            return item
        }

        function createSubMenu(label) {
            return subMenuComp.createObject(null, { "title": label })
        }

        function addSeparator() {
            contextMenu.addItem(menuSeparatorComp.createObject(null))
        }

        function handleCommand(cmd) {
            if (!TimelineBridge) return
            if (cmd.startsWith("add.")) {
                TimelineBridge.createObject(cmd.substring(4), contextClickFrame, contextClickLayer)
                return
            }
            switch (cmd) {
            case "edit.undo":   TimelineBridge.undo(); break
            case "edit.redo":   TimelineBridge.redo(); break
            case "clip.delete": TimelineBridge.deleteClip(targetClipId); break
            case "clip.split":  TimelineBridge.splitClip(targetClipId, contextClickFrame); break
            case "clip.cut":    TimelineBridge.cutClip(targetClipId); break
            case "clip.copy":   TimelineBridge.copyClip(targetClipId); break
            case "edit.paste":  TimelineBridge.pasteClip(contextClickFrame, contextClickLayer); break
            case "view.gridsettings": gridSettingsRequested(); break
            default: console.log("Unknown command:", cmd)
            }
        }

        function rebuildMenu() {
            while (contextMenu.count > 0) {
                var item = contextMenu.itemAt(0)
                contextMenu.removeItem(item)
                ;(function(obj) { Qt.callLater(() => { try { if (obj) obj.destroy() } catch(e){} }) })(item)
            }

            if (targetType === "timeline") {
                var objectMenu = createSubMenu("オブジェクトを追加")
                var objects    = TimelineBridge.getAvailableObjects()
                for (var i = 0; i < objects.length; i++) {
                    var obj     = objects[i]
                    var objItem = menuItemComp.createObject(null, { "text": obj.name, "iconName": "shape_line" })
                    ;(function(id) { objItem.triggered.connect(() => handleCommand("add." + id)) })(obj.id)
                    objectMenu.addItem(objItem)
                }
                contextMenu.addMenu(objectMenu)
                addSeparator()
                contextMenu.addItem(createMenuItem("元に戻す",   "edit.undo",         "arrow_go_back_line"))
                contextMenu.addItem(createMenuItem("やり直す",   "edit.redo",         "arrow_go_forward_line"))
                contextMenu.addItem(createMenuItem("貼り付け",   "edit.paste",        "clipboard_line"))
                addSeparator()
                contextMenu.addItem(createMenuItem("グリッド設定...", "view.gridsettings", "grid_line"))

            } else if (targetType === "clip") {
                contextMenu.addItem(createMenuItem("削除",   "clip.delete", "delete_bin_line"))
                contextMenu.addItem(createMenuItem("分割",   "clip.split",  "scissors_cut_line"))
                addSeparator()
                contextMenu.addItem(createMenuItem("切り取り", "clip.cut",  "scissors_line"))
                contextMenu.addItem(createMenuItem("コピー",   "clip.copy", "file_copy_line"))
                addSeparator()

                var effectMenu = createSubMenu("エフェクトを追加")
                var effects    = TimelineBridge.getAvailableEffects()
                for (var j = 0; j < effects.length; j++) {
                    var eff     = effects[j]
                    var effItem = menuItemComp.createObject(null, { "text": eff.name, "iconName": "magic_line" })
                    ;(function(cId, eId) { effItem.triggered.connect(() => TimelineBridge.addEffect(cId, eId)) })(targetClipId, eff.id)
                    effectMenu.addItem(effItem)
                }
                contextMenu.addMenu(effectMenu)

                var plugins = TimelineBridge.getAvailableAudioPlugins()
                if (plugins.length > 0) {
                    var audioMenu = createSubMenu("オーディオプラグイン")
                    for (var k = 0; k < plugins.length; k++) {
                        var plug     = plugins[k]
                        var plugItem = menuItemComp.createObject(null,
                            { "text": "[" + plug.format + "] " + plug.name, "iconName": "music_line" })
                        ;(function(cId, pId) { plugItem.triggered.connect(() => TimelineBridge.addAudioPlugin(cId, pId)) })(targetClipId, plug.id)
                        audioMenu.addItem(plugItem)
                    }
                    contextMenu.addMenu(audioMenu)
                }
            }
        }

        Component { id: menuItemComp;      Common.IconMenuItem {} }
        Component { id: subMenuComp;       Menu {}               }
        Component { id: menuSeparatorComp; MenuSeparator {}      }
    }
}
