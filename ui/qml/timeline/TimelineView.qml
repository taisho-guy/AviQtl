import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ScrollView {
    id: timelineViewRoot

    property alias flickable: timelineFlickable
    property alias contentX: timelineFlickable.contentX
    property alias contentY: timelineFlickable.contentY
    property int layerHeight: 30
    property int layerCount: 128
    property int clipResizeHandleWidth
    // コンテキストメニュー用プロパティ
    property int contextClickFrame: 0
    property int contextClickLayer: 0
    // スナップ設定など
    property bool enableSnap: SettingsManager.settings.enableSnap

    function clamp(v, lo, hi) {
        return Math.max(lo, Math.min(hi, v));
    }

    function snapFrame(frame) {
        if (!enableSnap)
            return frame;

        return Math.max(0, Math.round(frame));
    }

    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOn
    ScrollBar.vertical.policy: ScrollBar.AlwaysOn

    Flickable {
        id: timelineFlickable

        contentWidth: Math.max(width, 3600 * (TimelineBridge ? TimelineBridge.timelineScale : 1)) // 仮の長さ
        contentHeight: layerCount * layerHeight
        interactive: true

        // グリッド背景
        Item {
            width: Math.max(timelineFlickable.width, timelineFlickable.contentWidth)
            height: timelineFlickable.contentHeight

            Canvas {
                id: timelineGrid

                anchors.fill: parent
                // 必要に応じてグリッド線を描画
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    ctx.strokeStyle = Qt.rgba(0.5, 0.5, 0.5, 0.2);
                    ctx.lineWidth = 1;
                    // レイヤー区切り線
                    for (var i = 0; i < layerCount; i++) {
                        var y = i * layerHeight;
                        ctx.beginPath();
                        ctx.moveTo(0, y);
                        ctx.lineTo(width, y);
                        ctx.stroke();
                    }
                }
            }

        }

        // クリップ一覧
        Repeater {
            model: TimelineBridge ? TimelineBridge.clips : []

            delegate: Rectangle {
                id: clipRect

                property double scale: TimelineBridge ? TimelineBridge.timelineScale : 1

                x: modelData.startFrame * scale
                y: modelData.layer * layerHeight
                width: modelData.durationFrames * scale
                height: layerHeight - 2
                color: "#66aa99"
                border.color: "white"
                border.width: 1
                opacity: 0.8
                radius: 4

                Text {
                    anchors.centerIn: parent
                    text: modelData.type + " (" + modelData.id + ")"
                    color: "white"
                    font.pixelSize: 10
                }

                // クリップ操作（選択・移動）
                MouseArea {
                    id: moveArea

                    anchors.fill: parent
                    anchors.rightMargin: clipResizeHandleWidth
                    drag.target: clipRect
                    drag.axis: Drag.XAndYAxis
                    drag.smoothed: false
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onPressed: {
                        if (TimelineBridge)
                            TimelineBridge.selectClip(modelData.id);

                    }
                    onReleased: {
                        if (TimelineBridge) {
                            var newStart = timelineViewRoot.snapFrame(clipRect.x / clipRect.scale);
                            var newLayer = Math.round(clipRect.y / layerHeight);
                            TimelineBridge.updateClip(modelData.id, newLayer, newStart, modelData.durationFrames);
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
                            var frame = timelineViewRoot.snapFrame((clipRect.x + mouse.x) / clipRect.scale);
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
                                var currentScale = clipRect.scale;
                                var deltaFrames = Math.round(deltaX / currentScale);

                                // ガード: 持続時間が5フレーム未満にならないように
                                if (startDuration - deltaFrames < 5)
                                    return ;

                                // プレビュー更新 (即時反映)
                                var newStart = startFrame + deltaFrames;
                                var newDur = startDuration - deltaFrames;

                                clipRect.x = newStart * currentScale;
                                clipRect.width = newDur * currentScale;
                            }
                        }

                        onReleased: {
                            resizing = false;
                            if (TimelineBridge) {
                                var currentScale = clipRect.scale;
                                // 最終的な座標からフレームを逆算（スナップ含む）
                                var finalStart = timelineViewRoot.snapFrame(clipRect.x / currentScale);
                                // Startがズレた分、Durationを補正してEndを維持する
                                var endFrame = startFrame + startDuration;
                                var finalDur = endFrame - finalStart;

                                TimelineBridge.updateClip(modelData.id, modelData.layer, finalStart, finalDur);

                                // バインディングが切れたプロパティを復元
                                clipRect.x = Qt.binding(function() {
                                    return modelData.startFrame * clipRect.scale;
                                });
                                clipRect.width = Qt.binding(function() {
                                    return modelData.durationFrames * clipRect.scale;
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
                            startX = mouseX;
                            startWidth = clipRect.width;
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

                                clipRect.width = newW;
                            }
                        }
                        onReleased: {
                            resizing = false;
                            if (TimelineBridge) {
                                var scale = TimelineBridge.timelineScale;
                                var newDur = Math.round(clipRect.width / scale);
                                TimelineBridge.updateClip(modelData.id, modelData.layer, modelData.startFrame, newDur);
                            }
                            // バインディング復元
                            clipRect.width = Qt.binding(function() {
                                return modelData.durationFrames * clipRect.scale;
                            });
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

        function createMenuItem(label, cmd) {
            var item = menuItemComp.createObject(null, {
                "text": label
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
                if (WindowManager) {
                    WindowManager.systemSettingsVisible = true;
                    var win = WindowManager.getWindow("systemSettings");
                    if (win)
                        win.currentTabIndex = 2;

                }
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
                        "text": obj.name
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
                contextMenu.addItem(createMenuItem("元に戻す", "edit.undo"));
                contextMenu.addItem(createMenuItem("やり直す", "edit.redo"));
                contextMenu.addItem(createMenuItem("貼り付け", "edit.paste"));
                addSeparator();
                contextMenu.addItem(createMenuItem("グリッド設定...", "view.gridsettings"));
            } else if (targetType === "clip") {
                // クリップ右クリック
                contextMenu.addItem(createMenuItem("削除", "clip.delete"));
                contextMenu.addItem(createMenuItem("分割", "clip.split"));
                addSeparator();
                contextMenu.addItem(createMenuItem("切り取り", "clip.cut"));
                contextMenu.addItem(createMenuItem("コピー", "clip.copy"));
                addSeparator();
                // エフェクト追加メニュー
                var effectMenu = createSubMenu("エフェクトを追加");
                var effects = TimelineBridge.getAvailableEffects();
                for (var j = 0; j < effects.length; j++) {
                    var eff = effects[j];
                    var effItem = menuItemComp.createObject(null, {
                        "text": eff.name
                    });
                    (function(clipId, effId) {
                        effItem.triggered.connect(function() {
                            TimelineBridge.addEffect(clipId, effId);
                        });
                    })(targetClipId, eff.id);
                    effectMenu.addItem(effItem);
                }
                contextMenu.addMenu(effectMenu);
            }
        }

        Component {
            id: menuItemComp

            MenuItem {
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
