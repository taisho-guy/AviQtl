import "../common" as Common
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ScrollView {
    // "Auto", "BPM", "Frame"

    id: timelineViewRoot

    property alias flickable: timelineFlickable
    property alias contentX: timelineFlickable.contentX
    property alias contentY: timelineFlickable.contentY
    property int layerHeight: 30
    property int layerCount: 128
    property int clipResizeHandleWidth
    // レイヤーのロック状態を取得する関数（TimelineWindowから注入される）
    property var getLayerLocked: function(layer) {
        return false;
    }
    // コンテキストメニュー用プロパティ
    property int contextClickFrame: 0
    property int contextClickLayer: 0
    // スナップ設定など
    property bool enableSnap: SettingsManager.settings.enableSnap
    // 末尾に少し余白を持たせる（AviUtl的な「置きやすさ」）
    property int tailPaddingFrames: 120
    // === グリッド設定 (Parametric Grid State) ===
    property var gridSettings: ({
        "mode": "Auto",
        "bpm": 120,
        "offset": 0,
        "interval": 10,
        "subdivision": 4
    })
    // === 動的タイムライン長の計算 (AviUtl風) ===
    // クリップ配列の末尾を自動追跡する
    readonly property int maxClipEndFrame: {
        var maxEnd = 0;
        var clipList = (TimelineBridge && TimelineBridge.clips) ? TimelineBridge.clips : [];
        for (var i = 0; i < clipList.length; i++) {
            var clip = clipList[i];
            var end = clip.startFrame + clip.durationFrames;
            if (end > maxEnd)
                maxEnd = end;

        }
        return maxEnd;
    }
    // プロジェクト尺とクリップ末尾のうち長い方 + 余白
    readonly property int timelineLengthFrames: {
        return Math.max(100, maxClipEndFrame + tailPaddingFrames);
    }

    // グリッド設定画面の表示を親に要求するシグナル
    signal gridSettingsRequested()

    function clamp(v, lo, hi) {
        return Math.max(lo, Math.min(hi, v));
    }

    function snapFrame(frame) {
        if (!enableSnap)
            return frame;

        return Math.max(0, Math.round(frame));
    }

    onGridSettingsChanged: timelineGrid.requestPaint()
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOn
    ScrollBar.vertical.policy: ScrollBar.AlwaysOn

    Flickable {
        id: timelineFlickable

        contentWidth: Math.max(width, timelineLengthFrames * (TimelineBridge ? TimelineBridge.timelineScale : 1))
        contentHeight: layerCount * layerHeight
        interactive: true

        // 再描画トリガーの追加
        Connections {
            function onContentXChanged() {
                timelineGrid.requestPaint();
            }

            function onContentYChanged() {
                timelineGrid.requestPaint();
            }

            target: timelineFlickable
        }

        Connections {
            function onTimelineScaleChanged() {
                timelineGrid.requestPaint();
            }

            target: TimelineBridge ? TimelineBridge : null
        }

        // グリッド背景
        Item {
            // 最適化: コンテンツ全体ではなく、現在のビューポート(画面)サイズのみ確保する
            // Flickableの中身と一緒に動かないよう、位置をcontentX/Yに合わせて追従させる
            x: timelineFlickable.contentX
            y: timelineFlickable.contentY
            width: timelineFlickable.width
            height: timelineFlickable.height
            z: -1 // クリップの下に描画

            Canvas {
                // 1フレーム
                // 1秒
                // 10フレーム

                id: timelineGrid

                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    // Canvasをクリア (透明に)
                    ctx.clearRect(0, 0, width, height);
                    // 共通設定
                    ctx.lineWidth = 1;
                    // --- 1. レイヤー区切り線 (水平) ---
                    ctx.strokeStyle = Qt.rgba(0.5, 0.5, 0.5, 0.2);
                    var startY = timelineFlickable.contentY;
                    // レイヤー区切り線
                    for (var i = 0; i < layerCount; i++) {
                        // Canvasの(0,0)はViewportの左上なので、スクロール分を引いて描画位置を計算
                        var y = (i * layerHeight) - startY;
                        // 画面外ならスキップ
                        if (y < -layerHeight || y > height)
                            continue;

                        ctx.beginPath();
                        ctx.moveTo(0, y);
                        ctx.lineTo(width, y);
                        ctx.stroke();
                    }
                    // --- 2. 時間軸グリッド (垂直) ---
                    if (!TimelineBridge)
                        return ;

                    var scale = TimelineBridge.timelineScale;
                    var contentX = timelineFlickable.contentX;
                    var projectFps = (TimelineBridge.project && TimelineBridge.project.fps) ? TimelineBridge.project.fps : 60;
                    // 描画すべきフレーム範囲を計算
                    var startFrame = Math.floor(contentX / scale);
                    var endFrame = Math.ceil((contentX + width) / scale);
                    // === グリッド計算ロジック (Strategy) ===
                    var step = 1;
                    var offsetFrames = 0;
                    var isBpmMode = (gridSettings.mode === "BPM");
                    var bpmDiv = 1;
                    if (isBpmMode) {
                        // BPMモード: 1拍あたりのフレーム数を計算
                        var bps = gridSettings.bpm / 60;
                        var beatFrames = projectFps / bps; // 1拍のフレーム数 (小数含む)
                        // ズームに応じて分割 (1拍 -> 8分 -> 16分)
                        if (scale > 3)
                            bpmDiv = 4;
                        else if (scale > 1.5)
                            bpmDiv = 2;
                        step = beatFrames / bpmDiv;
                        offsetFrames = gridSettings.offset * projectFps;
                    } else if (gridSettings.mode === "Frame") {
                        // 固定フレームモード
                        step = gridSettings.interval;
                    } else {
                        // Auto (AviUtl風): ズームに応じて間引き
                        if (scale < 0.5)
                            step = Math.ceil(projectFps);
                        else if (scale < 1.5)
                            step = 10;
                        else if (scale < 3)
                            step = 5;
                        else
                            step = 1;
                    }
                    // グリッド線の描画ループ
                    // 数式: lineX = offset + n * step
                    // startFrame <= offset + n * step を満たす最小の n を求める
                    var startN = Math.ceil((startFrame - offsetFrames) / step);
                    var endN = Math.floor((endFrame - offsetFrames) / step);
                    for (var n = startN; n <= endN; n++) {
                        var f = offsetFrames + n * step;
                        // ローカル座標への変換: (フレーム位置 - 現在のスクロール位置)
                        var x = (f * scale) - contentX;
                        ctx.beginPath();
                        // 線のスタイリング
                        if (isBpmMode) {
                            // 小節の頭
                            var isMeasure = (n % (gridSettings.subdivision * bpmDiv) === 0);
                            // 拍の頭
                            var isBeat = (n % bpmDiv === 0);
                            if (isMeasure) {
                                ctx.strokeStyle = Qt.rgba(0.5, 0.8, 1, 0.5);
                                ctx.lineWidth = 1.5;
                            } else if (isBeat) {
                                ctx.strokeStyle = Qt.rgba(0.5, 0.5, 0.5, 0.3);
                                ctx.lineWidth = 1;
                            } else {
                                ctx.strokeStyle = Qt.rgba(0.5, 0.5, 0.5, 0.15);
                                ctx.lineWidth = 1;
                            }
                        } else {
                            // 通常モード
                            ctx.strokeStyle = Qt.rgba(0.5, 0.5, 0.5, 0.15);
                            ctx.lineWidth = 1;
                        }
                        ctx.moveTo(x, 0);
                        ctx.lineTo(x, height);
                        ctx.stroke();
                    }
                }
            }

        }

        // グループ制御の有効範囲表示 (カーテン)
        Item {
            id: groupControlCurtain

            property int selectedClipId: (TimelineBridge && TimelineBridge.selection) ? TimelineBridge.selection.selectedClipId : -1
            property var targetEffect: null
            property int layerCount: 0
            property var clipData: null

            function update() {
                targetEffect = null;
                clipData = null;
                layerCount = 0;
                if (selectedClipId < 0 || !TimelineBridge)
                    return ;

                // クリップデータを検索
                var clips = TimelineBridge.clips;
                for (var i = 0; i < clips.length; i++) {
                    if (clips[i].id === selectedClipId) {
                        clipData = clips[i];
                        break;
                    }
                }
                if (!clipData)
                    return ;

                // GroupControlエフェクトを検索
                var effects = TimelineBridge.getClipEffectsModel(selectedClipId);
                for (var j = 0; j < effects.length; j++) {
                    if (effects[j].id === "GroupControl") {
                        targetEffect = effects[j];
                        layerCount = targetEffect.params["layerCount"] || 0;
                        break;
                    }
                }
            }

            z: -0.5 // クリップの下、グリッドの上
            visible: targetEffect !== null && layerCount > 0
            // 位置とサイズ: クリップの開始時間、直下のレイヤーから開始
            x: clipData ? clipData.startFrame * (TimelineBridge ? TimelineBridge.timelineScale : 1) : 0
            y: clipData ? (clipData.layer + 1) * timelineViewRoot.layerHeight : 0
            width: clipData ? clipData.durationFrames * (TimelineBridge ? TimelineBridge.timelineScale : 1) : 0
            height: layerCount * timelineViewRoot.layerHeight

            Connections {
                function onSelectedClipIdChanged() {
                    groupControlCurtain.update();
                }

                target: TimelineBridge ? TimelineBridge.selection : null
            }

            Connections {
                function onClipsChanged() {
                    groupControlCurtain.update();
                }

                function onClipEffectsChanged(clipId) {
                    if (clipId === groupControlCurtain.selectedClipId)
                        groupControlCurtain.update();

                }

                target: TimelineBridge
            }

            Connections {
                function onParamsChanged() {
                    if (groupControlCurtain.targetEffect)
                        groupControlCurtain.layerCount = groupControlCurtain.targetEffect.params["layerCount"] || 0;

                }

                target: groupControlCurtain.targetEffect
                ignoreUnknownSignals: true
            }

            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(0, 0.8, 0, 0.15) // AviUtl風の緑色半透明
                border.color: Qt.rgba(0, 0.8, 0, 0.4)
                border.width: 1
            }

        }

        // クリップ一覧
        Repeater {
            model: TimelineBridge ? TimelineBridge.clips : []

            delegate: Item {
                id: clipDelegate

                property double scale: TimelineBridge ? TimelineBridge.timelineScale : 1
                // 選択状態を監視 (AviUtl風の視覚フィードバック)
                readonly property bool isSelected: {
                    return TimelineBridge && TimelineBridge.selection && TimelineBridge.selection.selectedClipId === modelData.id;
                }
                // レイヤーがロックされているか確認
                readonly property bool isLayerLocked: getLayerLocked(modelData.layer)
                // グループ制御情報
                property int groupLayerCount: 0
                property var groupEffectModel: null

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

                onIsSelectedChanged: updateGroupInfo()
                Component.onCompleted: updateGroupInfo()
                x: modelData.startFrame * scale
                y: modelData.layer * layerHeight
                width: modelData.durationFrames * scale
                height: layerHeight
                z: modelData.layer // レイヤー順に重ね合わせ

                Connections {
                    function onClipEffectsChanged(clipId) {
                        if (clipId === modelData.id)
                            updateGroupInfo();

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

                // グループ制御の有効範囲表示 (カーテン)
                Rectangle {
                    visible: clipDelegate.isSelected && clipDelegate.groupLayerCount > 0
                    x: 0
                    y: layerHeight
                    width: parent.width
                    height: clipDelegate.groupLayerCount * layerHeight
                    color: Qt.rgba(0, 0.8, 0, 0.15)
                    border.color: Qt.rgba(0, 0.8, 0, 0.4)
                    border.width: 1
                    z: -1 // クリップの下
                }

                Rectangle {
                    id: clipRect

                    anchors.fill: parent
                    anchors.bottomMargin: 2
                    // 選択時は背景を明るく、枠をオレンジに
                    color: isSelected ? "#77ccbb" : "#66aa99"
                    border.color: isSelected ? "#ff8800" : "#ffffff"
                    border.width: isSelected ? 2 : 1
                    opacity: 0.8
                    radius: 4

                    Text {
                        id: clipLabel
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.type + " (" + modelData.id + ")"
                        color: "white"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                        readonly property int padding: 4
                        property real stickyX: Math.max(0, timelineFlickable.contentX - clipDelegate.x)
                        x: Math.min(stickyX, parent.width - width - padding) + padding
                        width: Math.min(implicitWidth, parent.width - padding * 2)
                    }

                    // クリップ操作（選択・移動）
                    MouseArea {
                        id: moveArea

                        anchors.fill: parent
                        anchors.rightMargin: clipResizeHandleWidth
                        // ロックされたレイヤーではドラッグ不可
                        drag.target: clipDelegate.isLayerLocked ? null : clipDelegate
                        drag.axis: Drag.XAndYAxis
                        drag.smoothed: false
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: clipDelegate.isLayerLocked ? Qt.ForbiddenCursor : Qt.OpenHandCursor
                        onPressed: {
                            if (clipDelegate.isLayerLocked)
                                return ;

                            if (TimelineBridge)
                                TimelineBridge.selectClip(modelData.id);

                        }
                        onReleased: {
                            if (TimelineBridge) {
                                var newStart = timelineViewRoot.snapFrame(clipDelegate.x / clipDelegate.scale);
                                var newLayer = Math.round(clipDelegate.y / layerHeight);
                                TimelineBridge.updateClip(modelData.id, newLayer, newStart, modelData.durationFrames);
                                // バインディング復元
                                clipDelegate.x = Qt.binding(function() {
                                    return modelData.startFrame * clipDelegate.scale;
                                });
                                clipDelegate.y = Qt.binding(function() {
                                    return modelData.layer * layerHeight;
                                });
                            }
                        }
                        // 右クリックメニュー
                        onDoubleClicked: {
                            // ダブルクリックでオブジェクト設定ダイアログを開く
                            if (WindowManager)
                                WindowManager.raiseWindow("objectSettings");

                        }
                        onClicked: (mouse) => {
                            if (mouse.button === Qt.RightButton) {
                                var frame = timelineViewRoot.snapFrame((clipDelegate.x + mouse.x) / clipDelegate.scale);
                                contextMenu.openAt(mouse.x, mouse.y, "clip", frame, modelData.layer, modelData.id);
                            }
                        }
                    }

                    // クリップリサイズハンドル（左端） - [Optimized UX: Left Edge Resize]
                    Rectangle {
                        width: clipResizeHandleWidth
                        height: parent.height
                        anchors.left: parent.left
                        color: "transparent"
                        z: 10

                        MouseArea {
                            property int startX: 0
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

                                startX = mouseX;
                                startFrame = modelData.startFrame;
                                startDuration = modelData.durationFrames;
                                resizing = true;
                                if (TimelineBridge)
                                    TimelineBridge.selectClip(modelData.id);

                                mouse.accepted = true;
                            }
                            onPositionChanged: (mouse) => {
                                if (resizing) {
                                    var deltaX = mouseX - startX;
                                    var currentScale = clipDelegate.scale;
                                    var deltaFrames = Math.round(deltaX / currentScale);
                                    // ガード: 持続時間が5フレーム未満にならないように
                                    if (startDuration - deltaFrames < 5)
                                        return ;

                                    // プレビュー更新 (即時反映)
                                    var newStart = startFrame + deltaFrames;
                                    var newDur = startDuration - deltaFrames;
                                    clipDelegate.x = newStart * currentScale;
                                    clipDelegate.width = newDur * currentScale;
                                }
                            }
                            onReleased: {
                                resizing = false;
                                if (TimelineBridge) {
                                    var currentScale = clipDelegate.scale;
                                    // 最終的な座標からフレームを逆算（スナップ含む）
                                    var finalStart = timelineViewRoot.snapFrame(clipDelegate.x / currentScale);
                                    // Startがズレた分、Durationを補正してEndを維持する
                                    var endFrame = startFrame + startDuration;
                                    var finalDur = endFrame - finalStart;
                                    TimelineBridge.updateClip(modelData.id, modelData.layer, finalStart, finalDur);
                                    // バインディングが切れたプロパティを復元
                                    clipDelegate.x = Qt.binding(function() {
                                        return modelData.startFrame * clipDelegate.scale;
                                    });
                                    clipDelegate.width = Qt.binding(function() {
                                        return modelData.durationFrames * clipDelegate.scale;
                                    });
                                }
                            }
                        }

                    }

                    // クリップリサイズハンドル（右端）
                    Rectangle {
                        width: clipResizeHandleWidth
                        height: parent.height
                        anchors.right: parent.right
                        color: "transparent"

                        MouseArea {
                            property int startX: 0
                            property int startWidth: 0
                            property bool resizing: false

                            anchors.fill: parent
                            cursorShape: Qt.SizeHorCursor
                            hoverEnabled: true
                            preventStealing: true
                            onPressed: {
                                if (clipDelegate.isLayerLocked)
                                    return ;

                                startX = mouseX;
                                startWidth = clipDelegate.width;
                                resizing = true;
                                if (TimelineBridge)
                                    TimelineBridge.selectClip(modelData.id);

                            }
                            onPositionChanged: {
                                if (resizing) {
                                    var delta = mouseX - startX;
                                    var newW = startWidth + delta;
                                    if (newW < 5)
                                        newW = 5;

                                    clipDelegate.width = newW;
                                }
                            }
                            onReleased: {
                                resizing = false;
                                if (TimelineBridge) {
                                    var scale = TimelineBridge.timelineScale;
                                    var newDur = Math.round(clipDelegate.width / scale);
                                    TimelineBridge.updateClip(modelData.id, modelData.layer, modelData.startFrame, newDur);
                                }
                                // バインディング復元
                                clipDelegate.width = Qt.binding(function() {
                                    return modelData.durationFrames * clipDelegate.scale;
                                });
                            }
                        }

                    }

                }

            }

        }

        // 再生ヘッド
        Rectangle {
            id: playhead

            x: (TimelineBridge && TimelineBridge.transport ? TimelineBridge.transport.currentFrame : 0) * (TimelineBridge ? TimelineBridge.timelineScale : 1)
            y: 0
            width: 2
            height: parent.height
            color: "red"
            z: 100
        }

        // 背景操作（カーソル移動・コンテキストメニュー）
        MouseArea {
            anchors.fill: parent
            z: -1
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (mouse) => {
                var scale = TimelineBridge ? TimelineBridge.timelineScale : 1;
                var frame = timelineViewRoot.snapFrame(mouse.x / scale);
                if (mouse.button === Qt.LeftButton) {
                    if (TimelineBridge && TimelineBridge.transport)
                        TimelineBridge.transport.currentFrame = frame;

                } else if (mouse.button === Qt.RightButton) {
                    var layer = Math.floor(mouse.y / layerHeight);
                    contextMenu.openAt(mouse.x, mouse.y, "timeline", frame, layer, -1);
                }
            }
        }

        // ホイール操作（横スクロール変換ロジック）
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton // クリックは下の要素に通す
            onWheel: (wheel) => {
                var dy = wheel.angleDelta.y;
                if (wheel.pixelDelta && wheel.pixelDelta.y !== 0)
                    dy = wheel.pixelDelta.y * 10;

                var dx = wheel.angleDelta.x;
                if (wheel.pixelDelta && wheel.pixelDelta.x !== 0)
                    dx = wheel.pixelDelta.x * 10;

                if (wheel.modifiers & Qt.ShiftModifier) {
                    // Shift+ホイール: 縦スクロール
                    var nextY = timelineFlickable.contentY - dy;
                    var maxY = Math.max(0, timelineFlickable.contentHeight - timelineFlickable.height);
                    timelineFlickable.contentY = clamp(nextY, 0, maxY);
                } else {
                    // 通常: 横スクロール
                    // トラックパッド等で横成分(dx)が強ければそちらを優先
                    var delta = (Math.abs(dx) > Math.abs(dy)) ? dx : dy;
                    var nextX = timelineFlickable.contentX - delta;
                    var maxX = Math.max(0, timelineFlickable.contentWidth - timelineFlickable.width);
                    timelineFlickable.contentX = clamp(nextX, 0, maxX);
                }
                wheel.accepted = true;
            }
        }

    }

    // コンテキストメニュー（復元版）
    Menu {
        // 汎用アイコン

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
            item.triggered.connect(function() {
                handleCommand(cmd);
            });
            return item;
        }

        function createSubMenu(label) {
            var menu = subMenuComp.createObject(null, {
                "title": label
            });
            return menu;
        }

        function addSeparator() {
            var sep = menuSeparatorComp.createObject(null);
            contextMenu.addItem(sep);
        }

        function handleCommand(cmd) {
            if (!TimelineBridge)
                return ;

            // add.xxx 形式のコマンド（オブジェクト追加）
            if (cmd.startsWith("add.")) {
                var type = cmd.substring(4);
                TimelineBridge.createObject(type, contextClickFrame, contextClickLayer);
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
                TimelineBridge.deleteClip(targetClipId);
                break;
            case "clip.split":
                TimelineBridge.splitClip(targetClipId, contextClickFrame);
                break;
            case "clip.cut":
                TimelineBridge.cutClip(targetClipId);
                break;
            case "clip.copy":
                TimelineBridge.copyClip(targetClipId);
                break;
            case "edit.paste":
                TimelineBridge.pasteClip(contextClickFrame, contextClickLayer);
                break;
            case "view.gridsettings":
                gridSettingsRequested();
                break;
            default:
                console.log("Unknown command:", cmd);
            }
        }

        function rebuildMenu() {
            // 既存のアイテムをクリア
            while (contextMenu.count > 0) {
                var item = contextMenu.itemAt(0);
                contextMenu.removeItem(item);
                // スタイルがアイテムにアクセスする問題を回避するため破棄を遅延
                (function(obj) {
                    Qt.callLater(function() {
                        try {
                            if (obj)
                                obj.destroy();

                        } catch (e) {
                        }
                    });
                })(item);
            }
            if (targetType === "timeline") {
                // 背景右クリック：オブジェクト追加メニュー
                var objectMenu = createSubMenu("オブジェクトを追加");
                var objects = TimelineBridge.getAvailableObjects();
                for (var i = 0; i < objects.length; i++) {
                    var obj = objects[i];
                    var objItem = menuItemComp.createObject(null, {
                        "text": obj.name,
                        "iconName": "shape_line"
                    });
                    (function(id) {
                        objItem.triggered.connect(function() {
                            handleCommand("add." + id);
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
                // クリップ右クリック
                contextMenu.addItem(createMenuItem("削除", "clip.delete", "delete_bin_line"));
                contextMenu.addItem(createMenuItem("分割", "clip.split", "scissors_cut_line"));
                addSeparator();
                contextMenu.addItem(createMenuItem("切り取り", "clip.cut", "scissors_line"));
                contextMenu.addItem(createMenuItem("コピー", "clip.copy", "file_copy_line"));
                addSeparator();
                // エフェクト追加メニュー
                var effectMenu = createSubMenu("エフェクトを追加");
                var effects = TimelineBridge.getAvailableEffects();
                for (var j = 0; j < effects.length; j++) {
                    var eff = effects[j];
                    var effItem = menuItemComp.createObject(null, {
                        "text": eff.name,
                        "iconName": "magic_line"
                    });
                    (function(clipId, effId) {
                        effItem.triggered.connect(function() {
                            TimelineBridge.addEffect(clipId, effId);
                        });
                    })(targetClipId, eff.id);
                    effectMenu.addItem(effItem);
                }
                contextMenu.addMenu(effectMenu);
                // オーディオプラグイン追加メニュー
                var audioPluginMenu = createSubMenu("オーディオプラグイン");
                var plugins = TimelineBridge.getAvailableAudioPlugins();
                // 数が多い場合はサブメニュー化などを検討すべきだが、まずはフラットに表示
                for (var k = 0; k < plugins.length; k++) {
                    var plug = plugins[k];
                    var plugItem = menuItemComp.createObject(null, {
                        "text": "[" + plug.format + "] " + plug.name,
                        "iconName": "music_line"
                    });
                    (function(cId, pId) {
                        plugItem.triggered.connect(function() {
                            TimelineBridge.addAudioPlugin(cId, pId);
                        });
                    })(targetClipId, plug.id);
                    audioPluginMenu.addItem(plugItem);
                }
                if (plugins.length > 0)
                    contextMenu.addMenu(audioPluginMenu);

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
