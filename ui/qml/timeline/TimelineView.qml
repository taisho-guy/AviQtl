import "../common" as Common
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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

    function getGridInterval() {
        if (!TimelineBridge)
            return 1;

        var scale = TimelineBridge.timelineScale;
        var projectFps = (TimelineBridge.project && TimelineBridge.project.fps) ? TimelineBridge.project.fps : 60;
        var step = 1;
        if (gridSettings.mode === "BPM") {
            var bps = gridSettings.bpm / 60;
            var beatFrames = projectFps / bps;
            var bpmDiv = 1;
            if (scale > 3)
                bpmDiv = 4;
            else if (scale > 1.5)
                bpmDiv = 2;
            step = beatFrames / bpmDiv;
        } else if (gridSettings.mode === "Frame") {
            step = gridSettings.interval;
        } else {
            // Auto
            if (scale < 0.5)
                step = Math.ceil(projectFps);
            else if (scale < 1.5)
                step = 10;
            else if (scale < 3)
                step = 5;
            else
                step = 1;
        }
        return step;
    }

    function snapFrame(frame) {
        if (!enableSnap)
            return Math.round(frame);

        var step = getGridInterval();
        var offset = (gridSettings.mode === "BPM" && TimelineBridge && TimelineBridge.project) ? gridSettings.offset * TimelineBridge.project.fps : 0;
        var snapped = Math.round((frame - offset) / step) * step + offset;
        return Math.max(0, Math.round(snapped));
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
                    var step = timelineViewRoot.getGridInterval();
                    var offsetFrames = (gridSettings.mode === "BPM" && TimelineBridge.project) ? gridSettings.offset * TimelineBridge.project.fps : 0;
                    var isBpmMode = (gridSettings.mode === "BPM");
                    var bpmDiv = 1;
                    if (isBpmMode) {
                        if (scale > 3)
                            bpmDiv = 4;
                        else if (scale > 1.5)
                            bpmDiv = 2;
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
                // クリップ表示名
                property string clipDisplayName: modelData.type

                function updateClipDisplayName() {
                    if (modelData.type === "text" && TimelineBridge) {
                        // modelData.params は ClipModel から提供されるフラットなパラメータマップ
                        clipDisplayName = (modelData.params && modelData.params["text"]) ? modelData.params["text"] : "テキスト";
                        return ;
                    }
                    clipDisplayName = modelData.type;
                }

                // ドラッグ中の位置計算と更新を行う関数
                // ロジックを分離して可読性を向上
                function updateDragPosition(mouse) {
                    // 1. マウスの移動量(Delta)を計算
                    // mapToItemを使ってシーン全体の座標系で計算することで、
                    // Flickableのスクロールやアイテム自体の移動による座標ズレを防ぐ
                    var scenePos = mapToItem(timelineFlickable.contentItem, mouse.x, mouse.y);
                    var deltaX = scenePos.x - moveArea.dragStartScenePos.x;
                    var deltaY = scenePos.y - moveArea.dragStartScenePos.y;
                    // 2. 初期位置に移動量を加算して「希望の」位置を算出
                    var rawFrame = moveArea.initialFrame + (deltaX / clipDelegate.scale);
                    var proposedFrame = timelineViewRoot.snapFrame(rawFrame);
                    var proposedLayer = moveArea.initialLayer + Math.round(deltaY / layerHeight);
                    // 3. 値を有効な範囲に制限
                    proposedLayer = Math.max(0, Math.min(proposedLayer, timelineViewRoot.layerCount - 1));
                    proposedFrame = Math.max(0, proposedFrame);
                    // 4. 最適化: グリッド位置が変わっていなければ再計算しない
                    if (proposedFrame === moveArea.lastProposedFrame && proposedLayer === moveArea.lastProposedLayer)
                        return ;

                    moveArea.lastProposedFrame = proposedFrame;
                    moveArea.lastProposedLayer = proposedLayer;
                    // 5. 最終位置の決定（衝突解決含む）
                    var finalFrame = proposedFrame;
                    var finalLayer = proposedLayer;
                    // C++側の衝突解決ロジックを利用
                    if (TimelineBridge && typeof TimelineBridge.resolveDragPosition === "function") {
                        var finalPos = TimelineBridge.resolveDragPosition(modelData.id, proposedLayer, proposedFrame);
                        finalFrame = finalPos.x;
                        finalLayer = finalPos.y;
                    }
                    // 6. 衝突回避時のオートスクロール
                    // ドラッグによってクリップが強制的に移動させられた場合（衝突回避）、
                    // その移動分だけビューをスクロールさせて、マウス位置とクリップの相対関係を維持しようとする
                    if (finalFrame !== proposedFrame) {
                        var diff = (finalFrame - proposedFrame) * clipDelegate.scale;
                        timelineFlickable.contentX = Math.max(0, timelineFlickable.contentX + diff);
                    }
                    // 7. デリゲートの表示位置を更新
                    clipDelegate.x = finalFrame * clipDelegate.scale;
                    clipDelegate.y = finalLayer * layerHeight;
                }

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

                    // クリップ操作（選択・移動）
                    MouseArea {
                        id: moveArea

                        property point dragStartScenePos: Qt.point(0, 0)
                        property int initialLayer: 0
                        property int initialFrame: 0
                        property int lastProposedFrame: -1
                        property int lastProposedLayer: -1

                        anchors.fill: parent
                        anchors.rightMargin: clipResizeHandleWidth
                        // ロックされたレイヤーではドラッグ不可
                        // drag.target / drag.axis を削除し、生のイベントで制御する
                        // これによりQtのドラッグシステムによる閾値や補正の干渉を排除し、
                        // AviUtlライクな「マウスに完全に追従する」挙動を実現する
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: clipDelegate.isLayerLocked ? Qt.ForbiddenCursor : Qt.OpenHandCursor
                        preventStealing: true
                        onPressed: (mouse) => {
                            if (clipDelegate.isLayerLocked)
                                return ;

                            if (TimelineBridge)
                                TimelineBridge.selectClip(modelData.id);

                            // 開始時のシーン座標と、クリップの初期位置を記録
                            // これにより「移動量」ベースの計算を行い、ドラッグ中の座標系変動の影響を受けないようにする
                            var scenePos = mapToItem(timelineFlickable.contentItem, mouse.x, mouse.y);
                            dragStartScenePos = scenePos;
                            initialLayer = modelData.layer;
                            initialFrame = modelData.startFrame;
                            // キャッシュをリセット
                            lastProposedFrame = -1;
                            lastProposedLayer = -1;
                        }
                        onPositionChanged: (mouse) => {
                            if (pressed && !clipDelegate.isLayerLocked)
                                clipDelegate.updateDragPosition(mouse);

                        }
                        onReleased: {
                            if (TimelineBridge) {
                                var newStart = Math.round(clipDelegate.x / clipDelegate.scale);
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
                                    var rawNewStart = startFrame + (deltaX / currentScale);
                                    var newStart = timelineViewRoot.snapFrame(rawNewStart);
                                    var newDur = startDuration - (newStart - startFrame);
                                    // ガード: 持続時間が5フレーム未満にならないように
                                    if (newDur < 5)
                                        return ;

                                    // プレビュー更新 (即時反映)
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
                                    var rawWidth = startWidth + delta;
                                    var rawEndFrame = modelData.startFrame + (rawWidth / clipDelegate.scale);
                                    var snappedEndFrame = timelineViewRoot.snapFrame(rawEndFrame);
                                    var newDur = Math.max(5, snappedEndFrame - modelData.startFrame);
                                    clipDelegate.width = newDur * clipDelegate.scale;
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

    // オーディオプラグインメニュー (永続インスタンス)
    // 毎回生成すると重いため、一度だけ生成して使い回す
    Common.AudioPluginMenu {
        id: sharedAudioMenu

        onPluginSelected: function(pluginId) {
            TimelineBridge.addAudioPlugin(contextMenu.targetClipId, pluginId);
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
                // 永続メニューは破棄しない
                if (item === sharedAudioMenu)
                    continue;

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
                contextMenu.addMenu(sharedAudioMenu);
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
