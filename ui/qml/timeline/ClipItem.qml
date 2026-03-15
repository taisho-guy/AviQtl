import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: clipDelegate

    property int layerHeight: 30
    property int layerCount: 128
    property int clipResizeHandleWidth: 6
    property bool isBoxSelecting: false
    property var boxSelectionPreviewIds: []
    property Item flickableContentItem: null
    property var snapFrameFunc: function(f) {
        return f;
    }
    property int resizeDraftStart: -1
    property int resizeDraftDuration: -1
    property double scale: TimelineBridge ? TimelineBridge.timelineScale : 1
    property bool forceVisualSelection: false
    property var forcedSelectedIds: []
    readonly property bool committedSelected: ((TimelineBridge && TimelineBridge.selection) ? (TimelineBridge.selection.selectedClipIds.includes(modelData.id)) : false)
    readonly property bool previewSelected: isBoxSelecting && boxSelectionPreviewIds.includes(modelData.id)
    readonly property bool forcedSelected: forceVisualSelection && forcedSelectedIds.includes(modelData.id)
    readonly property bool isSelected: previewSelected || (forceVisualSelection ? forcedSelected : committedSelected)
    readonly property bool isLayerLocked: getLayerLocked(modelData.layer)
    property int groupLayerCount: 0
    property var groupEffectModel: null
    property string clipDisplayName: modelData.type

    signal clipSelected(int clipId, int modifiers, bool isSelected)
    signal clipMoved(int clipId, int newLayer, int newStartFrame, int duration)
    signal clipResized(int clipId, int newLayer, int newStartFrame, int newDuration)
    signal clipDoubleClicked(int clipId)

    function updateGroupInfo() {
        groupLayerCount = 0;
        groupEffectModel = null;
        if (!isSelected || !TimelineBridge)
            return ;

        var effects = TimelineBridge.getClipEffectsModel(modelData.id);
        for (var i = 0; i < effects.length; i++) {
            if (effects[i].id === "GroupControl") {
                groupEffectModel = effects[i];
                groupLayerCount = groupEffectModel.params["layerCount"] || 0;
                break;
            }
        }
    }

    property int dragDeltaStart: (isSelected && timelineViewRoot.isDraggingMulti) ? timelineViewRoot.activeDragDeltaFrame : 0
    property int dragDeltaLayer: (isSelected && timelineViewRoot.isDraggingMulti) ? timelineViewRoot.activeDragDeltaLayer : 0
    x: (resizeDraftStart >= 0 ? resizeDraftStart : Math.max(0, modelData.startFrame + dragDeltaStart)) * scale
    y: Math.max(0, modelData.layer + dragDeltaLayer) * layerHeight
    width: (resizeDraftDuration >= 0 ? resizeDraftDuration : modelData.durationFrames) * scale
    height: layerHeight
    z: modelData.layer
    onIsSelectedChanged: updateGroupInfo()
    Component.onCompleted: updateGroupInfo()

    Connections {
        function onClipEffectsChanged(clipId) {
            if (clipId === modelData.id)
                clipDelegate.updateGroupInfo();

        }

        target: TimelineBridge
    }

    Connections {
        function onParamsChanged() {
            if (clipDelegate.groupEffectModel)
                clipDelegate.groupLayerCount = clipDelegate.groupEffectModel.params["layerCount"] || 0;

        }

        target: clipDelegate.groupEffectModel
        ignoreUnknownSignals: true
    }

    Rectangle {
        visible: clipDelegate.isSelected && clipDelegate.groupLayerCount > 0
        x: 0
        y: layerHeight
        width: parent.width
        height: clipDelegate.groupLayerCount * layerHeight
        color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.18)
        border.color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.55)
        border.width: 1
        z: -1
    }

    Rectangle {
        id: clipRect

        anchors.fill: parent
        anchors.bottomMargin: 2
        color: {
            var c = TimelineBridge ? TimelineBridge.getClipTypeColor(modelData.type) : "";
            if (c !== "")
                return isSelected ? Qt.lighter(c, 1.3) : c;

            return isSelected ? palette.highlight : palette.mid;
        }
        border.color: isSelected ? palette.highlight : palette.midlight
        border.width: isSelected ? 2 : 1
        opacity: 0.8
        radius: 4

        Text {
            readonly property int padding: 4
            property real stickyX: Math.max(0, timelineFlickable.contentX - clipDelegate.x)

            anchors.verticalCenter: parent.verticalCenter
            text: clipDelegate.clipDisplayName + " (" + modelData.id + ")"
            color: "white"
            font.pixelSize: 10
            elide: Text.ElideRight
            x: Math.min(stickyX, parent.width - width - padding) + padding
            width: Math.min(implicitWidth, parent.width - padding * 2)
        }

        Canvas {
            id: waveformCanvas

            readonly property bool isAudio: modelData.type === "audio"
            property int waveRev: 0
            property bool _paintPending: false
            readonly property int displayDuration: {
                if (clipDelegate.resizeDraftDuration >= 0)
                    return clipDelegate.resizeDraftDuration;

                return modelData.durationFrames;
            }

            function _schedulePaint() {
                if (!isAudio)
                    return ;

                if (_paintPending)
                    return ;

                _paintPending = true;
                Qt.callLater(function() {
                    _paintPending = false;
                    waveformCanvas.requestPaint();
                });
            }

            anchors.fill: parent
            visible: isAudio
            opacity: 0.7
            onWidthChanged: _schedulePaint()
            onDisplayDurationChanged: _schedulePaint()
            onWaveRevChanged: _schedulePaint()
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                if (!isAudio || width <= 0 || !TimelineBridge)
                    return ;

                var pw = Math.floor(width);
                var dur = displayDuration;
                if (pw <= 0 || dur <= 0)
                    return ;

                var peaks = TimelineBridge.getWaveformPeaks(modelData.id, pw, dur);
                if (!peaks || peaks.length === 0)
                    return ;

                var cy = height / 2;
                var maxH = cy - 2;
                ctx.strokeStyle = "rgba(255, 255, 255, 0.9)";
                ctx.lineWidth = 1;
                for (var i = 0; i < peaks.length; i++) {
                    var h = Math.max(1, peaks[i] * maxH);
                    ctx.beginPath();
                    ctx.moveTo(i + 0.5, cy - h);
                    ctx.lineTo(i + 0.5, cy + h);
                    ctx.stroke();
                }
            }

            Connections {
                function onClipsChanged() {
                    waveformCanvas._schedulePaint();
                }

                target: TimelineBridge
            }

        }

        MouseArea {
            // Handled by parent if needed, or emit signal to request scroll
            // timelineFlickable.contentX = Math.max(0, timelineFlickable.contentX + (finalF - propF) * clipDelegate.scale);

            id: moveArea

            property point dragStartScenePos: Qt.point(0, 0)
            property point pressScenePos: Qt.point(0, 0)
            property int initialLayer: 0
            property int initialFrame: 0
            property int lastProposedFrame: -1
            property int lastProposedLayer: -1
            property bool dragActive: false
            property real dragThreshold: 3

            anchors.fill: parent
            anchors.leftMargin: clipResizeHandleWidth
            anchors.rightMargin: clipResizeHandleWidth
            acceptedButtons: Qt.LeftButton
            cursorShape: clipDelegate.isLayerLocked ? Qt.ForbiddenCursor : Qt.OpenHandCursor
            preventStealing: true
            onPressed: (mouse) => {
                if (clipDelegate.isLayerLocked)
                    return ;

                dragActive = false;

                // UX Fix: If the clip being dragged is NOT selected, select it immediately before dragging
                if (!clipDelegate.isSelected) {
                    if (!(mouse.modifiers & Qt.ControlModifier)) {
                        clipSelected(modelData.id, mouse.modifiers, false);
                    }
                }

                var sp = mapToItem(flickableContentItem, mouse.x, mouse.y);
                pressScenePos = sp;
                dragStartScenePos = sp;
                initialLayer = modelData.layer;
                initialFrame = modelData.startFrame;
                lastProposedFrame = -1;
                lastProposedLayer = -1;
            }
            onPositionChanged: (mouse) => {
                if (!pressed || clipDelegate.isLayerLocked)
                    return ;

                var sp = mapToItem(flickableContentItem, mouse.x, mouse.y);
                if (!dragActive) {
                    if (Math.abs(sp.x - pressScenePos.x) < dragThreshold && Math.abs(sp.y - pressScenePos.y) < dragThreshold)
                        return ;

                    dragActive = true;
                    timelineViewRoot.isDraggingMulti = true;
                    timelineViewRoot.activeDragDeltaFrame = 0;
                    timelineViewRoot.activeDragDeltaLayer = 0;
                }
                var dX = sp.x - dragStartScenePos.x;
                var dY = sp.y - dragStartScenePos.y;
                var rawF = initialFrame + dX / clipDelegate.scale;
                var propF = snapFrameFunc(rawF);
                var propL = Math.max(0, Math.min(initialLayer + Math.round(dY / layerHeight), layerCount - 1));
                propF = Math.max(0, propF);
                if (propF === lastProposedFrame && propL === lastProposedLayer)
                    return ;

                lastProposedFrame = propF;
                lastProposedLayer = propL;
                var finalF = propF, finalL = propL;
                if (TimelineBridge && typeof TimelineBridge.resolveDragPosition === "function") {
                    var pos = TimelineBridge.resolveDragPosition(modelData.id, propL, propF);
                    finalF = pos.x;
                    finalL = pos.y;
                }

                timelineViewRoot.activeDragDeltaFrame = finalF - initialFrame;
                timelineViewRoot.activeDragDeltaLayer = finalL - initialLayer;
            }
            onReleased: (mouse) => {
                if (!TimelineBridge)
                    return ;

                if (!dragActive) {
                    clipSelected(modelData.id, mouse.modifiers, clipDelegate.isSelected);
                    return ;
                }

                var deltaF = timelineViewRoot.activeDragDeltaFrame;
                var deltaL = timelineViewRoot.activeDragDeltaLayer;

                timelineViewRoot.isDraggingMulti = false;
                dragActive = false;

                // Emit relative movement so TimelineView can move all selected clips
                clipMoved(modelData.id, deltaL, deltaF, modelData.durationFrames);
            }
            onDoubleClicked: {
                if (WindowManager)
                    WindowManager.raiseWindow("objectSettings");

            }
        }

        Rectangle {
            width: clipResizeHandleWidth
            height: parent.height
            anchors.left: parent.left
            color: "transparent"
            z: 10

            MouseArea {
                id: leftResizeArea

                property real startSceneX: 0 // Flickable contentItem 座標
                property int startFrame: 0
                property int startDuration: 0
                property bool resizing: false

                anchors.fill: parent
                cursorShape: Qt.SizeHorCursor
                hoverEnabled: true
                preventStealing: true
                onPressed: (mouse) => {
                    if (clipDelegate.isLayerLocked)
                        return ;

                    if (!clipDelegate.isSelected && !(mouse.modifiers & Qt.ControlModifier)) {
                        clipSelected(modelData.id, mouse.modifiers, false);
                    }

                    var sp = mapToItem(flickableContentItem, mouse.x, mouse.y);
                    startSceneX = sp.x;
                    startFrame = modelData.startFrame;
                    startDuration = modelData.durationFrames;
                    resizing = true;
                    mouse.accepted = true;
                }
                onPositionChanged: (mouse) => {
                    if (!resizing)
                        return ;

                    var sp = mapToItem(flickableContentItem, mouse.x, mouse.y);
                    var delta = sp.x - startSceneX;
                    // 右端（終点）を固定して左端のみ動かす
                    var endFrame = startFrame + startDuration;
                    var rawNewStart = startFrame + delta / clipDelegate.scale;
                    var newStart = Math.max(0, snapFrameFunc(rawNewStart));
                    var newDur = endFrame - newStart;
                    var minDur = SettingsManager ? SettingsManager.value("minClipDurationFrames", 5) : 5;
                    if (newDur < minDur) {
                        newStart = endFrame - minDur;
                        newDur = minDur;
                    }
                    // ドラフトプロパティ経由で表示更新（バインディング破壊なし）
                    clipDelegate.resizeDraftStart = newStart;
                    clipDelegate.resizeDraftDuration = newDur;
                }
                onReleased: {
                    if (!resizing)
                        return ;

                    resizing = false;
                    if (TimelineBridge && clipDelegate.resizeDraftDuration > 0) {
                        var newStart = clipDelegate.resizeDraftStart >= 0 ? clipDelegate.resizeDraftStart : modelData.startFrame;
                        var deltaStart = newStart - startFrame;
                        var deltaDuration = clipDelegate.resizeDraftDuration - startDuration;
                        clipResized(modelData.id, deltaStart, deltaDuration, 0); // using params to pass delta
                    }

                    // ドラフト解除 → バインディングが自動で正値を返す
                    clipDelegate.resizeDraftStart = -1;
                    clipDelegate.resizeDraftDuration = -1;
                }
            }

        }

        Rectangle {
            width: clipResizeHandleWidth
            height: parent.height
            anchors.right: parent.right
            color: "transparent"
            z: 10

            MouseArea {
                id: rightResizeArea

                property real startSceneX: 0 // Flickable contentItem 座標
                property int startFrame: 0 // リサイズ開始時の startFrame
                property int startDuration: 0
                property bool resizing: false

                anchors.fill: parent
                cursorShape: Qt.SizeHorCursor
                hoverEnabled: true
                preventStealing: true
                onPressed: (mouse) => {
                    if (clipDelegate.isLayerLocked)
                        return ;

                    if (!clipDelegate.isSelected && !(mouse.modifiers & Qt.ControlModifier)) {
                        clipSelected(modelData.id, mouse.modifiers, false);
                    }

                    var sp = mapToItem(flickableContentItem, mouse.x, mouse.y);
                    startSceneX = sp.x;
                    startFrame = modelData.startFrame;
                    startDuration = modelData.durationFrames;
                    resizing = true;
                    mouse.accepted = true;
                }
                onPositionChanged: (mouse) => {
                    if (!resizing)
                        return ;

                    var sp = mapToItem(flickableContentItem, mouse.x, mouse.y);
                    var delta = sp.x - startSceneX;
                    // 右端フレームをスナップ
                    var rawEndFrame = startFrame + (startDuration * clipDelegate.scale + delta) / clipDelegate.scale;
                    var snappedEndFrame = snapFrameFunc(rawEndFrame);
                    var minDur = SettingsManager ? SettingsManager.value("minClipDurationFrames", 5) : 5;
                    var newDur = Math.max(minDur, snappedEndFrame - startFrame);
                    // ドラフトプロパティ経由で表示更新（バインディング破壊なし）
                    clipDelegate.resizeDraftDuration = newDur;
                }
                onReleased: {
                    if (!resizing)
                        return ;

                    resizing = false;
                    if (TimelineBridge && clipDelegate.resizeDraftDuration > 0) {
                        var deltaDuration = clipDelegate.resizeDraftDuration - startDuration;
                        clipResized(modelData.id, 0, deltaDuration, 0);
                    }

                    // ドラフト解除 → バインディングが自動で正値を返す
                    clipDelegate.resizeDraftDuration = -1;
                }
            }

        }

    }

}
