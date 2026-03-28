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
    property int clipResizeHandleWidth: 6
    property var getLayerLocked: function(layer) {
        return false;
    }
    property int contextClickFrame: 0
    property int contextClickLayer: 0
    property bool boxSelecting: false
    property point boxSelectionStart: Qt.point(0, 0)
    property point boxSelectionCurrent: Qt.point(0, 0)
    property real boxSelectionThreshold: 6
    property bool boxSelectionAdditive: false
    property var boxSelectionPreviewIds: []
    property int activeDragDeltaFrame: 0
    property bool autoScrollSuspended: false
    property int activeDragDeltaLayer: 0
    property bool isDraggingMulti: false
    property int selectionMinFrame: 0
    property int selectionMinLayer: 0
    property int selectionMaxLayer: 0
    property bool dragAutoScrollActive: false
    property point dragViewportPos: Qt.point(-1, -1)
    property real dragScrollEdge: 48
    property real dragScrollStep: 24
    property var dragAutoScrollCallback: null
    property var currentSceneData: {
        if (!TimelineBridge || !TimelineBridge.scenes)
            return null;

        for (var i = 0; i < TimelineBridge.scenes.length; i++) {
            if (TimelineBridge.scenes[i].id === TimelineBridge.currentSceneId)
                return TimelineBridge.scenes[i];

        }
        return null;
    }
    property bool enableSnap: currentSceneData && currentSceneData.enableSnap !== undefined ? currentSceneData.enableSnap : (SettingsManager.settings ? SettingsManager.settings.enableSnap : true)
    property int magneticSnapRange: currentSceneData && currentSceneData.magneticSnapRange !== undefined ? currentSceneData.magneticSnapRange : (SettingsManager.settings ? timelineViewRoot.magneticSnapRange : 10)
    property int tailPaddingFrames: 120
    property var gridSettings: {
        if (currentSceneData)
            return {
            "mode": currentSceneData.gridMode || "Auto",
            "bpm": currentSceneData.gridBpm || 120,
            "offset": currentSceneData.gridOffset || 0,
            "interval": currentSceneData.gridInterval || 10,
            "subdivision": currentSceneData.gridSubdivision || 4
        };

        return {
            "mode": "Auto",
            "bpm": 120,
            "offset": 0,
            "interval": 10,
            "subdivision": 4
        };
    }
    readonly property int maxClipEndFrame: {
        var maxEnd = 0;
        var clipList = (TimelineBridge && TimelineBridge.clips) ? TimelineBridge.clips : [];
        for (var i = 0; i < clipList.length; i++) {
            var end = clipList[i].startFrame + clipList[i].durationFrames;
            if (end > maxEnd)
                maxEnd = end;

        }
        return maxEnd;
    }
    readonly property int timelineLengthFrames: Math.max(100, maxClipEndFrame + tailPaddingFrames)

    function beginDragAutoScroll(callback) {
        dragAutoScrollCallback = callback;
        dragAutoScrollActive = true;
    }

    function updateDragAutoScroll(posInViewport) {
        dragViewportPos = posInViewport;
    }

    function endDragAutoScroll() {
        dragAutoScrollActive = false;
        dragAutoScrollCallback = null;
    }

    function effectiveSelectionIds() {
        if (TimelineBridge && TimelineBridge.selection)
            return TimelineBridge.selection.selectedClipIds.slice(0);

        return [];
    }

    function handleClipSelection(clipId, modifiers, isSelected) {
        if (!TimelineBridge || !TimelineBridge.selection)
            return ;

        if (modifiers & Qt.ControlModifier)
            TimelineBridge.toggleSelection(clipId, {
            });
        else
            TimelineBridge.applySelectionIds([clipId]);
    }

    function clipHitsBox(clip, frameA, frameB, layerA, layerB) {
        var minFrame = Math.min(frameA, frameB);
        var maxFrame = Math.max(frameA, frameB);
        var minLayer = Math.min(layerA, layerB);
        var maxLayer = Math.max(layerA, layerB);
        var clipStart = clip.startFrame;
        var clipEnd = clip.startFrame + clip.durationFrames;
        var frameOverlap = clipStart < maxFrame && minFrame < clipEnd;
        var layerMatch = clip.layer >= minLayer && clip.layer <= maxLayer;
        return frameOverlap && layerMatch;
    }

    function updateBoxSelectionPreview() {
        var scale = TimelineBridge ? TimelineBridge.timelineScale : 1;
        var x1 = Math.min(boxSelectionStart.x, boxSelectionCurrent.x);
        var x2 = Math.max(boxSelectionStart.x, boxSelectionCurrent.x);
        var y1 = Math.min(boxSelectionStart.y, boxSelectionCurrent.y);
        var y2 = Math.max(boxSelectionStart.y, boxSelectionCurrent.y);
        var frameA = Math.floor(x1 / scale);
        var frameB = Math.ceil(x2 / scale);
        var layerA = Math.max(0, Math.floor(y1 / layerHeight));
        var layerB = Math.max(0, Math.floor(y2 / layerHeight));
        var ids = [];
        if (boxSelectionAdditive)
            ids = effectiveSelectionIds();

        if (TimelineBridge && TimelineBridge.clips) {
            for (var j = 0; j < TimelineBridge.clips.length; j++) {
                var c = TimelineBridge.clips[j];
                if (clipHitsBox(c, frameA, frameB, layerA, layerB) && !ids.includes(c.id))
                    ids.push(c.id);

            }
        }
        boxSelectionPreviewIds = ids;
    }

    function clamp(v, lo, hi) {
        return Math.max(lo, Math.min(hi, v));
    }

    function getGridInterval() {
        if (!TimelineBridge)
            return 1;

        var scale = TimelineBridge.timelineScale;
        var projectFps = (TimelineBridge.project && TimelineBridge.project.fps) ? TimelineBridge.project.fps : 60;
        if (gridSettings.mode === "BPM") {
            var beatFrames = projectFps / (gridSettings.bpm / 60);
            var bpmDiv = scale > 3 ? 4 : scale > 1.5 ? 2 : 1;
            return beatFrames / bpmDiv;
        }
        if (gridSettings.mode === "Frame")
            return gridSettings.interval;

        // Auto
        if (scale < 0.5)
            return Math.ceil(projectFps);

        if (scale < 1.5)
            return 10;

        if (scale < 3)
            return 5;

        return 1;
    }

    function snapFrame(frame, ignoreSnap) {
        if (!enableSnap || ignoreSnap)
            return Math.round(frame);

        // グリッド無視時は整数丸めのみ
        var step = getGridInterval();
        var offset = (gridSettings.mode === "BPM" && TimelineBridge && TimelineBridge.project) ? gridSettings.offset * TimelineBridge.project.fps : 0;
        return Math.max(0, Math.round((Math.round((frame - offset) / step) * step) + offset));
    }

    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOn
    ScrollBar.vertical.policy: ScrollBar.AlwaysOn

    Flickable {
        // unified loop handles viewport updates now
        id: timelineFlickable

        contentWidth: Math.max(width, timelineLengthFrames * (TimelineBridge ? TimelineBridge.timelineScale : 1))
        contentHeight: layerCount * layerHeight
        interactive: true
        onMovementStarted: timelineViewRoot.autoScrollSuspended = true

        Timer {
            id: renderTimer

            interval: 16
            repeat: true
            running: true // Unified render loop
            onTriggered: {
                if (!TimelineBridge)
                    return ;

                // 1. Viewport sync
                if (typeof TimelineBridge.updateViewport === "function")
                    TimelineBridge.updateViewport(timelineFlickable.contentX, timelineFlickable.contentY);

                if (timelineViewRoot.ScrollBar.horizontal && timelineViewRoot.ScrollBar.horizontal.pressed)
                    timelineViewRoot.autoScrollSuspended = true;

                if (timelineViewRoot.ScrollBar.vertical && timelineViewRoot.ScrollBar.vertical.pressed)
                    timelineViewRoot.autoScrollSuspended = true;

                // 2. Playhead auto-scroll (Page turn)
                if (TimelineBridge.transport && TimelineBridge.transport.isPlaying && !timelineViewRoot.autoScrollSuspended) {
                    let viewportWidth = timelineFlickable.width;
                    let playheadX = TimelineBridge.transport.currentFrame * TimelineBridge.timelineScale;
                    let left = timelineFlickable.contentX;
                    let right = left + viewportWidth;
                    let margin = 24;
                    if (playheadX < left || playheadX >= right - margin) {
                        let nextPage = Math.floor(playheadX / Math.max(1, viewportWidth));
                        let maxX = Math.max(0, timelineFlickable.contentWidth - viewportWidth);
                        timelineFlickable.contentX = clamp(nextPage * viewportWidth, 0, maxX);
                    }
                }
                // 3. Drag auto-scroll
                if (timelineViewRoot.dragAutoScrollActive) {
                    let dx = 0;
                    let dy = 0;
                    let edge = timelineViewRoot.dragScrollEdge;
                    let step = timelineViewRoot.dragScrollStep;
                    if (timelineViewRoot.dragViewportPos.x < edge)
                        dx = -step;
                    else if (timelineViewRoot.dragViewportPos.x > timelineFlickable.width - edge)
                        dx = step;
                    if (timelineViewRoot.dragViewportPos.y < edge)
                        dy = -step;
                    else if (timelineViewRoot.dragViewportPos.y > timelineFlickable.height - edge)
                        dy = step;
                    if (dx !== 0 || dy !== 0) {
                        let maxX = Math.max(0, timelineFlickable.contentWidth - timelineFlickable.width);
                        let maxY = Math.max(0, timelineFlickable.contentHeight - timelineFlickable.height);
                        timelineFlickable.contentX = clamp(timelineFlickable.contentX + dx, 0, maxX);
                        timelineFlickable.contentY = clamp(timelineFlickable.contentY + dy, 0, maxY);
                        if (timelineViewRoot.dragAutoScrollCallback)
                            timelineViewRoot.dragAutoScrollCallback();

                    }
                }
            }
        }

        Connections {
            function onTimelineScaleChanged() {
            }

            target: TimelineBridge ?? null
        }

        Connections {
            function onIsPlayingChanged() {
                if (TimelineBridge.transport.isPlaying)
                    timelineViewRoot.autoScrollSuspended = false;

            }

            target: TimelineBridge && TimelineBridge.transport ? TimelineBridge.transport : null
        }

        Item {
            x: timelineFlickable.contentX
            y: timelineFlickable.contentY
            width: timelineFlickable.width
            height: timelineFlickable.height
            z: -1

            TimelineGrid {
                id: timelineGrid

                anchors.fill: parent
                width: timelineFlickable.width
                height: timelineFlickable.height
                contentX: timelineFlickable.contentX
                contentY: timelineFlickable.contentY
                gridInterval: timelineViewRoot.getGridInterval()
                layerCount: timelineViewRoot.layerCount
                layerHeight: timelineViewRoot.layerHeight
                gridSettings: timelineViewRoot.gridSettings
            }

        }

        Repeater {
            model: TimelineBridge ? TimelineBridge.clips : []

            delegate: ClipItem {
                layerHeight: timelineViewRoot.layerHeight
                layerCount: timelineViewRoot.layerCount
                clipResizeHandleWidth: timelineViewRoot.clipResizeHandleWidth
                isBoxSelecting: timelineViewRoot.boxSelecting
                boxSelectionPreviewIds: timelineViewRoot.boxSelectionPreviewIds
                forceVisualSelection: false
                forcedSelectedIds: []
                flickableContentItem: timelineFlickable.contentItem
                snapFrameFunc: timelineViewRoot.snapFrame
                onClipSelected: (clipId, modifiers, isSelected) => {
                    timelineViewRoot.handleClipSelection(clipId, modifiers, isSelected);
                }
                onClipMoved: (clipId, deltaLayer, deltaStart, unused) => {
                    if (TimelineBridge) {
                        var selectedIds = effectiveSelectionIds();
                        if (selectedIds.includes(clipId)) {
                            var moves = [];
                            for (var i = 0; i < TimelineBridge.clips.length; i++) {
                                var c = TimelineBridge.clips[i];
                                if (selectedIds.includes(c.id)) {
                                    var newL = Math.round(Number(c.layer) + Number(deltaLayer));
                                    var newF = Math.round(Number(c.startFrame) + Number(deltaStart));
                                    if (newL < 0)
                                        newL = 0;

                                    if (newL >= timelineViewRoot.layerCount)
                                        newL = timelineViewRoot.layerCount - 1;

                                    if (newF < 0)
                                        newF = 0;

                                    moves.push({
                                        "id": Number(c.id),
                                        "layer": newL,
                                        "startFrame": newF,
                                        "duration": Number(c.durationFrames)
                                    });
                                }
                            }
                            TimelineBridge.applyClipBatchMove(moves);
                        } else {
                            // Should not happen with new UX fix, but fallback
                            var c = TimelineBridge.clips.find((c) => {
                                return c.id === clipId;
                            });
                            if (c)
                                TimelineBridge.updateClip(clipId, Math.max(0, c.layer + deltaLayer), Math.max(0, c.startFrame + deltaStart), c.durationFrames);

                        }
                    }
                }
                onClipResized: (clipId, deltaStart, deltaDuration, unused) => {
                    if (TimelineBridge) {
                        if (TimelineBridge && TimelineBridge.selection && TimelineBridge.selection.selectedClipIds.includes(clipId)) {
                            TimelineBridge.resizeSelectedClips(deltaStart, deltaDuration);
                        } else {
                            var c = TimelineBridge.clips.find((c) => {
                                return c.id === clipId;
                            });
                            if (c)
                                TimelineBridge.updateClip(clipId, c.layer, Math.max(0, c.startFrame + deltaStart), Math.max(1, c.durationFrames + deltaDuration));

                        }
                    }
                }
                onClipDoubleClicked: (clipId) => {
                    if (WindowManager)
                        WindowManager.raiseWindow("objectSettings");

                }
            }

        }

        Rectangle {
            id: playhead

            x: (TimelineBridge && TimelineBridge.transport ? TimelineBridge.transport.currentFrame : 0) * (TimelineBridge ? TimelineBridge.timelineScale : 1)
            y: 0
            width: 2
            height: parent.height
            color: palette.highlight
            z: 100
        }

        MouseArea {
            anchors.fill: parent
            z: -1
            acceptedButtons: Qt.LeftButton
            onReleased: (mouse) => {
                var scale = TimelineBridge ? TimelineBridge.timelineScale : 1;
                var frame = timelineViewRoot.snapFrame(mouse.x / scale);
                if (TimelineBridge && TimelineBridge.transport)
                    TimelineBridge.transport.setCurrentFrame_seek(frame);

            }
        }

        MouseArea {
            anchors.fill: parent
            z: 999
            acceptedButtons: Qt.RightButton
            preventStealing: true
            onPressed: (mouse) => {
                boxSelecting = false;
                boxSelectionStart = mapToItem(timelineFlickable.contentItem, mouse.x, mouse.y);
                boxSelectionCurrent = boxSelectionStart;
                boxSelectionAdditive = !!(mouse.modifiers & Qt.ControlModifier);
                boxSelectionPreviewIds = [];
            }
            onPositionChanged: (mouse) => {
                boxSelectionCurrent = mapToItem(timelineFlickable.contentItem, mouse.x, mouse.y);
                if (Math.abs(boxSelectionCurrent.x - boxSelectionStart.x) >= boxSelectionThreshold || Math.abs(boxSelectionCurrent.y - boxSelectionStart.y) >= boxSelectionThreshold) {
                    boxSelecting = true;
                    timelineViewRoot.updateBoxSelectionPreview();
                }
            }
            onReleased: (mouse) => {
                var scale = TimelineBridge ? TimelineBridge.timelineScale : 1;
                var frame = timelineViewRoot.snapFrame(mouse.x / scale);
                if (!boxSelecting) {
                    var layer = Math.floor(mouse.y / layerHeight);
                    var clickedClipId = -1;
                    if (TimelineBridge && TimelineBridge.clips) {
                        for (var i = TimelineBridge.clips.length - 1; i >= 0; i--) {
                            var c = TimelineBridge.clips[i];
                            if (c.layer === layer && frame >= c.startFrame && frame < c.startFrame + c.durationFrames) {
                                clickedClipId = c.id;
                                break;
                            }
                        }
                    }
                    var currentSel = effectiveSelectionIds();
                    if (clickedClipId >= 0 && !currentSel.includes(clickedClipId))
                        TimelineBridge.applySelectionIds([clickedClipId]);

                    contextMenu.openAt(mouse.x, mouse.y, clickedClipId >= 0 ? "clip" : "timeline", frame, layer, clickedClipId);
                    return ;
                }
                // Update current position to exactly where the mouse was released
                boxSelectionCurrent = mapToItem(timelineFlickable.contentItem, mouse.x, mouse.y);
                timelineViewRoot.updateBoxSelectionPreview();
                var finalIds = boxSelectionPreviewIds.slice(0);
                TimelineBridge.applySelectionIds(finalIds);
                boxSelecting = false;
                boxSelectionPreviewIds = [];
            }
        }

        Rectangle {
            visible: boxSelecting
            z: 1000
            color: "#3388aaff"
            border.color: "#88aaff"
            border.width: 1
            x: Math.min(boxSelectionStart.x, boxSelectionCurrent.x)
            y: Math.min(boxSelectionStart.y, boxSelectionCurrent.y)
            width: Math.abs(boxSelectionCurrent.x - boxSelectionStart.x)
            height: Math.abs(boxSelectionCurrent.y - boxSelectionStart.y)
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            onWheel: (wheel) => {
                timelineViewRoot.autoScrollSuspended = true;
                var dy = (wheel.pixelDelta && wheel.pixelDelta.y !== 0) ? wheel.pixelDelta.y * 10 : wheel.angleDelta.y;
                var dx = (wheel.pixelDelta && wheel.pixelDelta.x !== 0) ? wheel.pixelDelta.x * 10 : wheel.angleDelta.x;
                if (wheel.modifiers & Qt.AltModifier || wheel.modifiers & Qt.ControlModifier) {
                    // Zoom
                    if (TimelineBridge) {
                        var step = SettingsManager ? SettingsManager.value("timelineZoomStep", 10) : 10;
                        var minZ = SettingsManager ? SettingsManager.value("timelineZoomMin", 10) : 10;
                        var maxZ = SettingsManager ? SettingsManager.value("timelineZoomMax", 400) : 400;
                        var direction = (Math.abs(dy) > Math.abs(dx) ? dy : dx) > 0 ? 1 : -1;
                        var newScale = TimelineBridge.timelineScale + (direction * step / 100);
                        newScale = clamp(newScale, minZ / 100, maxZ / 100);
                        // Zoom keeping the mouse position stationary if possible
                        var contentX = timelineFlickable.contentX;
                        var mouseX = wheel.x;
                        var frameAtMouse = (contentX + mouseX) / TimelineBridge.timelineScale;
                        TimelineBridge.timelineScale = newScale;
                        // Adjust scroll to keep frameAtMouse at mouseX
                        var newContentX = frameAtMouse * newScale - mouseX;
                        var maxX = Math.max(0, timelineFlickable.contentWidth - timelineFlickable.width);
                        timelineFlickable.contentX = clamp(newContentX, 0, maxX);
                    }
                } else if (wheel.modifiers & Qt.ShiftModifier) {
                    // Vertical Scroll
                    var maxY = Math.max(0, timelineFlickable.contentHeight - timelineFlickable.height);
                    timelineFlickable.contentY = clamp(timelineFlickable.contentY - dy, 0, maxY);
                } else {
                    // Horizontal Scroll
                    var delta = (Math.abs(dx) > Math.abs(dy)) ? dx : dy;
                    var maxX = Math.max(0, timelineFlickable.contentWidth - timelineFlickable.width);
                    timelineFlickable.contentX = clamp(timelineFlickable.contentX - delta, 0, maxX);
                }
                wheel.accepted = true;
            }
        }

    }

    Menu {
        id: contextMenu

        property string targetType: ""
        property int targetClipId: -1

        function openAt(x, y, type, frame, layer, clipId) {
            targetType = type;
            targetClipId = clipId;
            contextClickFrame = frame;
            contextClickLayer = layer;
            rebuildMenu();
            popup();
        }

        function createMenuItem(label, cmd, icon) {
            var item = menuItemComp.createObject(null, {
                "text": label,
                "iconName": icon || ""
            });
            item.triggered.connect(() => {
                return handleCommand(cmd);
            });
            return item;
        }

        function createSubMenu(label) {
            return subMenuComp.createObject(null, {
                "title": label
            });
        }

        function addSeparator() {
            contextMenu.addItem(menuSeparatorComp.createObject(null));
        }

        function shouldApplyToSelection() {
            if (!TimelineBridge || !TimelineBridge.selection || targetClipId < 0)
                return false;

            var ids = TimelineBridge.selection.selectedClipIds;
            if (!ids || ids.length <= 1)
                return false;

            for (var i = 0; i < ids.length; i++) {
                if (ids[i] === targetClipId)
                    return true;

            }
            return false;
        }

        function handleCommand(cmd) {
            if (!TimelineBridge)
                return ;

            if (cmd.startsWith("add.")) {
                TimelineBridge.createObject(cmd.substring(4), contextClickFrame, contextClickLayer);
                return ;
            }
            switch (cmd) {
            case "edit.undo":
                TimelineBridge.undo();
                break;
            case "edit.redo":
                TimelineBridge.redo();
                break;
            case "clip.delete":
                if (shouldApplyToSelection())
                    TimelineBridge.deleteSelectedClips();
                else
                    TimelineBridge.deleteClip(targetClipId);
                break;
            case "clip.split":
                TimelineBridge.splitClip(targetClipId, contextClickFrame);
                break;
            case "clip.cut":
                if (shouldApplyToSelection())
                    TimelineBridge.cutSelectedClips();
                else
                    TimelineBridge.cutClip(targetClipId);
                break;
            case "clip.copy":
                if (shouldApplyToSelection())
                    TimelineBridge.copySelectedClips();
                else
                    TimelineBridge.copyClip(targetClipId);
                break;
            case "edit.paste":
                TimelineBridge.pasteClip(contextClickFrame, contextClickLayer);
                break;
            case "view.gridsettings":
                break;
            default:
                console.log("Unknown command:", cmd);
            }
        }

        function rebuildMenu() {
            while (contextMenu.count > 0) {
                var item = contextMenu.itemAt(0);
                contextMenu.removeItem(item);
                (function(obj) {
                    Qt.callLater(() => {
                        try {
                            if (obj)
                                obj.destroy();

                        } catch (e) {
                        }
                    });
                })(item);
            }
            if (targetType === "timeline") {
                var objectMenu = createSubMenu("オブジェクトを追加");
                var objects = TimelineBridge.getAvailableObjects();
                for (var i = 0; i < objects.length; i++) {
                    var obj = objects[i];
                    var objItem = menuItemComp.createObject(null, {
                        "text": obj.name,
                        "iconName": "shape_line"
                    });
                    (function(id) {
                        objItem.triggered.connect(() => {
                            return handleCommand("add." + id);
                        });
                    })(obj.id);
                    objectMenu.addItem(objItem);
                }
                contextMenu.addMenu(objectMenu);
                addSeparator();
                contextMenu.addItem(createMenuItem("元に戻す", "edit.undo", "arrow_go_back_line"));
                contextMenu.addItem(createMenuItem("やり直す", "edit.redo", "arrow_go_forward_line"));
                contextMenu.addItem(createMenuItem("貼り付け", "edit.paste", "clipboard_line"));
                addSeparator();
                contextMenu.addItem(createMenuItem("グリッド設定...", "view.gridsettings", "grid_line"));
            } else if (targetType === "clip") {
                contextMenu.addItem(createMenuItem("削除", "clip.delete", "delete_bin_line"));
                contextMenu.addItem(createMenuItem("分割", "clip.split", "scissors_cut_line"));
                addSeparator();
                contextMenu.addItem(createMenuItem("切り取り", "clip.cut", "scissors_line"));
                contextMenu.addItem(createMenuItem("コピー", "clip.copy", "file_copy_line"));
            }
        }

        Component {
            id: menuItemComp

            Common.IconMenuItem {
            }

        }

        Component {
            id: subMenuComp

            Menu {
            }

        }

        Component {
            id: menuSeparatorComp

            MenuSeparator {
            }

        }

    }

}
