import Qt.labs.platform as Platform
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "common" as Common

ApplicationWindow {
    // Global Shortcuts

    id: mainWin

    visible: true
    width: 640
    height: 360
    x: 100
    y: 100
    title: "Rina - プレビュー"
    // お前はこれで死ねぇ！！！！！
    onClosing: (close) => {
        if (WindowManager)
            WindowManager.requestQuit();

        close.accepted = true;
    }
    // 起動時に自分自身(Window)をコントローラーに渡す
    Component.onCompleted: {
        if (TimelineBridge)
            TimelineBridge.setCompositeView(compositeView);

    }

    // 末尾到達時に一時停止するだけのシンプルなロジック
    Connections {
        function onCurrentFrameChanged() {
            if (TimelineBridge && TimelineBridge.transport) {
                if (TimelineBridge.transport.totalFrames > 0 && TimelineBridge.transport.currentFrame >= TimelineBridge.transport.totalFrames)
                    TimelineBridge.transport.pause();

            }
        }

        target: TimelineBridge ? TimelineBridge.transport : null
    }

    FontLoader {
        source: "qrc:/resources/remixicon.ttf"
    }

    // アクション定義 (ショートカット用)
    Action {
        id: newAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("project.new", "Ctrl+N") : "Ctrl+N"

        text: "新規プロジェクト"
        onTriggered: console.log("New Project")
    }

    Action {
        id: saveProjectAction // プロジェクトの上書き保存用アクション

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("project.save", "Ctrl+S") : "Ctrl+S"

        text: "プロジェクトの上書き保存"
        onTriggered: {
            if (TimelineBridge) {
                // 現在のプロジェクトパスが未設定の場合は名前を付けて保存ダイアログを開く
                if (TimelineBridge.currentProjectUrl === "")
                    saveDialog.open();
                else
                    TimelineBridge.saveProject("");
            }
        }
    }

    Action {
        id: loadAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("project.open", "Ctrl+O") : "Ctrl+O"

        text: "プロジェクトを開く"
        onTriggered: loadDialog.open()
    }

    Action {
        id: saveAsProjectAction // プロジェクトを名前を付けて保存用アクション

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("project.saveAs", "Ctrl+Shift+S") : "Ctrl+Shift+S"

        text: "プロジェクトを名前を付けて保存..."
        onTriggered: saveDialog.open()
    }

    Action {
        id: quitAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("app.quit", "Ctrl+Q") : "Ctrl+Q"

        text: "終了"
        onTriggered: {
            if (WindowManager)
                WindowManager.requestQuit();

        }
    }

    Action {
        id: undoAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("edit.undo", "Ctrl+Z") : "Ctrl+Z"

        text: "元に戻す"
        onTriggered: {
            if (TimelineBridge)
                TimelineBridge.undo();

        }
    }

    Action {
        id: redoAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("edit.redo", "Ctrl+Shift+Z") : "Ctrl+Shift+Z"

        text: "やり直す"
        onTriggered: {
            if (TimelineBridge)
                TimelineBridge.redo();

        }
    }

    Action {
        id: playPauseAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("transport.playPause", "Space") : "Space"

        text: "再生 / 一時停止"
        onTriggered: {
            if (TimelineBridge)
                TimelineBridge.togglePlay();

        }
    }

    Action {
        id: splitAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("timeline.split", "S") : "S"

        text: "クリップを分割"
        onTriggered: {
            if (TimelineBridge && TimelineBridge.transport) {
                var f = TimelineBridge.transport.currentFrame;
                if (TimelineBridge.selection && TimelineBridge.selection.selectedClipId >= 0) {
                    TimelineBridge.splitClip(TimelineBridge.selection.selectedClipId, f);
                } else {
                    if (TimelineBridge.selection && TimelineBridge.selection.selectedClipIds.length > 0) {
                        for (var i = 0; i < TimelineBridge.selection.selectedClipIds.length; i++) {
                            TimelineBridge.splitClip(TimelineBridge.selection.selectedClipIds[i], f);
                        }
                    }
                }
            }
        }
    }

    Action {
        id: deleteAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("edit.delete", "Delete") : "Delete"

        text: "削除"
        onTriggered: {
            if (TimelineBridge)
                TimelineBridge.deleteSelectedClips();

        }
    }

    Action {
        id: copyAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("edit.copy", "Ctrl+C") : "Ctrl+C"

        text: "コピー"
        onTriggered: {
            if (TimelineBridge)
                TimelineBridge.copySelectedClips();

        }
    }

    Action {
        id: cutAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("edit.cut", "Ctrl+X") : "Ctrl+X"

        text: "カット"
        onTriggered: {
            if (TimelineBridge)
                TimelineBridge.cutSelectedClips();

        }
    }

    Action {
        id: pasteAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("edit.paste", "Ctrl+V") : "Ctrl+V"

        text: "貼り付け"
        onTriggered: {
            if (TimelineBridge && TimelineBridge.transport) {
                var f = TimelineBridge.transport.currentFrame;
                var l = TimelineBridge.selectedLayer !== undefined ? TimelineBridge.selectedLayer : 0;
                TimelineBridge.pasteClip(f, l);
            }
        }
    }

    Action {
        id: duplicateAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("edit.duplicate", "Ctrl+D") : "Ctrl+D"

        text: "複製"
        onTriggered: {
            if (TimelineBridge) {
                TimelineBridge.copySelectedClips();
                var f = TimelineBridge.transport ? TimelineBridge.transport.currentFrame : 0;
                var l = TimelineBridge.selectedLayer !== undefined ? TimelineBridge.selectedLayer : 0;
                TimelineBridge.pasteClip(f, l);
            }
        }
    }

    Action {
        id: nextFrameAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("transport.nextFrame", "Right") : "Right"

        text: "1フレーム進む"
        onTriggered: {
            if (TimelineBridge && TimelineBridge.transport)
                TimelineBridge.transport.currentFrame = Math.min(TimelineBridge.transport.currentFrame + 1, TimelineBridge.transport.totalFrames);

        }
    }

    Action {
        id: prevFrameAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("transport.prevFrame", "Left") : "Left"

        text: "1フレーム戻る"
        onTriggered: {
            if (TimelineBridge && TimelineBridge.transport)
                TimelineBridge.transport.currentFrame = Math.max(TimelineBridge.transport.currentFrame - 1, 0);

        }
    }

    Action {
        id: jumpStartAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("transport.jumpStart", "Home") : "Home"

        text: "先頭へ移動"
        onTriggered: {
            if (TimelineBridge && TimelineBridge.transport)
                TimelineBridge.transport.currentFrame = 0;

        }
    }

    Action {
        id: jumpEndAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("transport.jumpEnd", "End") : "End"

        text: "末尾へ移動"
        onTriggered: {
            if (TimelineBridge && TimelineBridge.transport)
                TimelineBridge.transport.currentFrame = TimelineBridge.transport.totalFrames;

        }
    }

    Action {
        id: zoomInAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("view.zoomIn", "Ctrl++") : "Ctrl++"

        text: "ズームイン"
        onTriggered: {
            if (TimelineBridge) {
                var step = SettingsManager ? SettingsManager.value("timelineZoomStep", 10) : 10;
                var maxZ = SettingsManager ? SettingsManager.value("timelineZoomMax", 400) : 400;
                TimelineBridge.timelineScale = Math.min(TimelineBridge.timelineScale + step / 100, maxZ / 100);
            }
        }
    }

    Action {
        id: zoomOutAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("view.zoomOut", "Ctrl+-") : "Ctrl+-"

        text: "ズームアウト"
        onTriggered: {
            if (TimelineBridge) {
                var step = SettingsManager ? SettingsManager.value("timelineZoomStep", 10) : 10;
                var minZ = SettingsManager ? SettingsManager.value("timelineZoomMin", 10) : 10;
                TimelineBridge.timelineScale = Math.max(TimelineBridge.timelineScale - step / 100, minZ / 100);
            }
        }
    }

    Action {
        id: moveUpAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("timeline.moveUp", "Alt+Up") : "Alt+Up"

        text: "レイヤーを上へ移動"
        onTriggered: {
            if (TimelineBridge)
                TimelineBridge.moveSelectedClips(-1, 0);

        }
    }

    Action {
        id: moveDownAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("timeline.moveDown", "Alt+Down") : "Alt+Down"

        text: "レイヤーを下へ移動"
        onTriggered: {
            if (TimelineBridge)
                TimelineBridge.moveSelectedClips(1, 0);

        }
    }

    Action {
        id: nudgeLeftAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("timeline.nudgeLeft", "Alt+Left") : "Alt+Left"

        text: "1フレーム左へ移動"
        onTriggered: {
            if (TimelineBridge)
                TimelineBridge.moveSelectedClips(0, -1);

        }
    }

    Action {
        id: nudgeRightAction

        property string shortcutText: SettingsManager ? SettingsManager.shortcut("timeline.nudgeRight", "Alt+Right") : "Alt+Right"

        text: "1フレーム右へ移動"
        onTriggered: {
            if (TimelineBridge)
                TimelineBridge.moveSelectedClips(0, 1);

        }
    }

    Platform.MessageDialog {
        id: errorDialog

        title: "エラー"
        buttons: Platform.MessageDialog.Ok
    }

    Connections {
        function onErrorOccurred(message) {
            errorDialog.text = message;
            errorDialog.open();
        }

        target: TimelineBridge
    }

    // FPSと再生速度の同期設定
    Binding {
        target: TimelineBridge ? TimelineBridge.transport : null
        property: "fps"
        value: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.fps : 60
    }

    Connections {
        function onPlaybackSpeedChanged() {
            if (TimelineBridge)
                TimelineBridge.syncPlaybackSpeed();

        }

        target: TimelineBridge ? TimelineBridge.transport : null
    }

    Connections {
        function onSampleRateChanged() {
            if (TimelineBridge)
                TimelineBridge.updateAudioSampleRate();

        }

        target: TimelineBridge ? TimelineBridge.project : null
    }

    Platform.FileDialog {
        id: saveDialog

        fileMode: Platform.FileDialog.SaveFile
        nameFilters: ["Rina Project files (*.rina)", "JSON files (*.json)"]
        defaultSuffix: "rina"
        onAccepted: {
            if (TimelineBridge)
                TimelineBridge.saveProject(file);

        }
    }

    Platform.FileDialog {
        id: loadDialog

        fileMode: Platform.FileDialog.OpenFile
        nameFilters: ["Rina Project files (*.rina)", "JSON files (*.json)"]
        onAccepted: {
            if (TimelineBridge)
                TimelineBridge.loadProject(file);

        }
    }

    ExportDialog {
        id: exportDialog
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        CompositeView {
            id: compositeView

            Layout.fillWidth: true
            Layout.fillHeight: true
            // タイムラインウィンドウからレイヤー状態を取得
            getLayerVisible: function(layer) {
                var tlWin = WindowManager.getWindow("timeline");
                if (tlWin && tlWin.getLayerVisible)
                    return tlWin.getLayerVisible(layer);

                return true;
            }
        }

        // 再生コントロールバー
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: mainWin.palette.window

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 10

                // シークバー
                Slider {
                    // 動かしている間は currentFrame を更新するがプレビューは止まる
                    // 離した瞬間にプレビュー確定
                    // 押した瞬間も同期
                    // シーク後は絶対に一時停止のままにするため、endScrub() は呼ばない

                    id: seekSlider

                    Layout.fillWidth: true
                    from: 0
                    to: {
                        // クリップの末尾を取得して動的に拡張
                        var maxEnd = 0;
                        var clipList = (TimelineBridge && TimelineBridge.clips) ? TimelineBridge.clips : [];
                        for (var j = 0; j < clipList.length; j++) {
                            var clip = clipList[j];
                            var end = clip.startFrame + clip.durationFrames;
                            if (end > maxEnd)
                                maxEnd = end;

                        }
                        return Math.max(1, maxEnd + 1);
                    }
                    onPressedChanged: {
                        if (TimelineBridge && TimelineBridge.transport) {
                            TimelineBridge.transport.isScrubbing = pressed;
                            if (pressed)
                                TimelineBridge.transport.beginScrub();
                            else
                                TimelineBridge.transport.setCurrentFrame_seek(Math.floor(value));
                        }
                    }
                    onMoved: {
                        if (TimelineBridge && TimelineBridge.transport)
                            TimelineBridge.transport.scrubTo(Math.floor(value));

                    }

                    // スクラブ中はUI側からの書き換えを優先し、通常時はトランスポートに同期する
                    Binding on value {
                        when: !seekSlider.pressed
                        value: (TimelineBridge && TimelineBridge.transport) ? TimelineBridge.transport.currentFrame : 0
                        restoreMode: Binding.RestoreBinding
                    }

                }

                Label {
                    // 総フレーム数の桁数に合わせて0埋めし、等幅フォントを適用する
                    text: {
                        var total = Math.floor(seekSlider.to);
                        var cur = Math.floor(seekSlider.value);
                        var digits = String(total).length;
                        // padStartを使用して先頭を0で埋める
                        return String(cur).padStart(digits, "0") + " / " + String(total);
                    }
                    font.family: "Monospace" // 等幅フォントの強制
                    font.features: {
                        "tnum": 1
                    } // OpenType tabular numerals
                    font.pixelSize: 12
                    color: mainWin.palette.text
                }

                // 操作ボタン
                RowLayout {
                    spacing: 0

                    Button {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        flat: true
                        onClicked: {
                            if (TimelineBridge && TimelineBridge.transport)
                                TimelineBridge.transport.setCurrentFrame_seek(Math.max(0, TimelineBridge.transport.currentFrame - 1));

                        }

                        contentItem: Common.RinaIcon {
                            iconName: "arrow_left_s_line"
                            size: 24
                            color: parent.hovered ? parent.palette.highlight : parent.palette.text
                        }

                    }

                    Button {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        flat: true
                        onClicked: {
                            if (TimelineBridge && TimelineBridge.transport)
                                TimelineBridge.transport.togglePlay();

                        }

                        contentItem: Common.RinaIcon {
                            iconName: (TimelineBridge && TimelineBridge.transport && TimelineBridge.transport.isPlaying) ? "pause_fill" : "play_fill"
                            size: 24
                            color: parent.hovered ? parent.palette.highlight : parent.palette.text
                        }

                    }

                    Button {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        flat: true
                        onClicked: {
                            if (TimelineBridge && TimelineBridge.transport)
                                TimelineBridge.transport.setCurrentFrame_seek(TimelineBridge.transport.currentFrame + 1);

                        }

                        contentItem: Common.RinaIcon {
                            iconName: "arrow_right_s_line"
                            size: 24
                            color: parent.hovered ? parent.palette.highlight : parent.palette.text
                        }

                    }

                }

                // 再生速度
                RowLayout {
                    spacing: 5

                    Label {
                        text: "速度"
                        color: mainWin.palette.text
                        font.pixelSize: 12
                    }

                    SpinBox {
                        from: SettingsManager ? SettingsManager.value("timelineZoomMin", 10) : 10
                        to: SettingsManager ? SettingsManager.value("timelineZoomMax", 400) : 400
                        stepSize: SettingsManager ? SettingsManager.value("timelineZoomStep", 10) : 10
                        editable: true
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 28
                        enabled: !(TimelineBridge && TimelineBridge.transport && TimelineBridge.transport.isPlaying)
                        // 値のバインディング
                        value: (TimelineBridge && TimelineBridge.transport) ? Math.round(TimelineBridge.transport.playbackSpeed * 100) : 100
                        onValueModified: {
                            if (TimelineBridge && TimelineBridge.transport)
                                TimelineBridge.transport.playbackSpeed = value / 100;

                        }
                        textFromValue: function(value, locale) {
                            return (value / 100).toFixed(1) + "x";
                        }
                        valueFromText: function(text, locale) {
                            return Number.fromLocaleString(locale, text.replace("x", "")) * 100;
                        }
                    }

                }

            }

        }

    }

    // Global Shortcuts
    Shortcut {
        sequence: newAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: newAction.trigger()
    }

    Shortcut {
        sequence: saveProjectAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: saveProjectAction.trigger()
    }

    Shortcut {
        sequence: loadAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: loadAction.trigger()
    }

    Shortcut {
        sequence: saveAsProjectAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: saveAsProjectAction.trigger()
    }

    Shortcut {
        sequence: quitAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: quitAction.trigger()
    }

    Shortcut {
        sequence: undoAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: undoAction.trigger()
    }

    Shortcut {
        sequence: redoAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: redoAction.trigger()
    }

    Shortcut {
        sequence: playPauseAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: playPauseAction.trigger()
    }

    Shortcut {
        sequence: splitAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: splitAction.trigger()
    }

    Shortcut {
        sequence: deleteAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: deleteAction.trigger()
    }

    Shortcut {
        sequence: copyAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: copyAction.trigger()
    }

    Shortcut {
        sequence: cutAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: cutAction.trigger()
    }

    Shortcut {
        sequence: pasteAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: pasteAction.trigger()
    }

    Shortcut {
        sequence: duplicateAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: duplicateAction.trigger()
    }

    Shortcut {
        sequence: nextFrameAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: nextFrameAction.trigger()
    }

    Shortcut {
        sequence: prevFrameAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: prevFrameAction.trigger()
    }

    Shortcut {
        sequence: jumpStartAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: jumpStartAction.trigger()
    }

    Shortcut {
        sequence: jumpEndAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: jumpEndAction.trigger()
    }

    Shortcut {
        sequence: zoomInAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: zoomInAction.trigger()
    }

    Shortcut {
        sequence: zoomOutAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: zoomOutAction.trigger()
    }

    Shortcut {
        sequence: moveUpAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: moveUpAction.trigger()
    }

    Shortcut {
        sequence: moveDownAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: moveDownAction.trigger()
    }

    Shortcut {
        sequence: nudgeLeftAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: nudgeLeftAction.trigger()
    }

    Shortcut {
        sequence: nudgeRightAction.shortcutText
        context: Qt.ApplicationShortcut
        onActivated: nudgeRightAction.trigger()
    }

    // View3D の背後に黒背景を強制しない
    background: Rectangle {
        color: mainWin.palette.window
    }

    menuBar: MenuBar {
        Menu {
            title: "ファイル"

            Common.IconMenuItem {
                action: newAction
                iconName: "file_add_line"
            }

            Common.IconMenuItem {
                action: loadAction
                iconName: "folder_open_line"
            }

            MenuSeparator {
            }

            Common.IconMenuItem {
                action: saveProjectAction
                iconName: "save_line"
            }

            Common.IconMenuItem {
                action: saveAsProjectAction
                iconName: "save_3_line"
            }

            MenuSeparator {
            }

            Common.IconMenuItem {
                text: "メディアの書き出し..."
                iconName: "movie_line"
                enabled: TimelineBridge && TimelineBridge.project
                onTriggered: {
                    exportDialog.x = mainWin.x + (mainWin.width - exportDialog.width) / 2;
                    exportDialog.y = mainWin.y + (mainWin.height - exportDialog.height) / 2;
                    exportDialog.open();
                }
            }

            MenuSeparator {
            }

            Common.IconMenuItem {
                text: "環境設定"
                iconName: "settings_3_line"
                onTriggered: {
                    if (WindowManager)
                        WindowManager.systemSettingsVisible = true;

                }
            }

            MenuSeparator {
            }

            Common.IconMenuItem {
                text: "終了"
                action: quitAction
                iconName: "close_circle_line"
            }

        }

        Menu {
            title: "フィルタ"

            Common.IconMenuItem {
                text: "エフェクトの追加"
                iconName: "magic_line"
                enabled: TimelineBridge && TimelineBridge.selection && TimelineBridge.selection.selectedClipId >= 0
                onTriggered: {
                    if (WindowManager)
                        WindowManager.objectSettingsVisible = true;

                }
            }

        }

        Menu {
            title: "設定"

            Common.IconMenuItem {
                text: "サイズの変更"
                iconName: "aspect_ratio_line"
                onTriggered: {
                    if (WindowManager)
                        WindowManager.projectSettingsVisible = true;

                }
            }

            Common.IconMenuItem {
                text: "フレームレートの変更"
                iconName: "speed_line"
                onTriggered: {
                    if (WindowManager)
                        WindowManager.projectSettingsVisible = true;

                }
            }

            MenuSeparator {
            }

            Common.IconMenuItem {
                text: "環境設定"
                iconName: "settings_3_line"
                onTriggered: {
                    if (WindowManager)
                        WindowManager.systemSettingsVisible = true;

                }
            }

        }

        Menu {
            title: "編集"

            Common.IconMenuItem {
                action: undoAction
                iconName: "arrow_go_back_line"
            }

            Common.IconMenuItem {
                action: redoAction
                iconName: "arrow_go_forward_line"
            }

        }

        Menu {
            title: "表示"

            Common.IconMenuItem {
                text: "再生ウィンドウの表示"
                iconName: "play_circle_line"
                onTriggered: mainWin.requestActivate()
            }

            Common.IconMenuItem {
                text: "タイムラインの表示"
                iconName: "layout_bottom_line"
                onTriggered: {
                    if (WindowManager)
                        WindowManager.timelineVisible = true;

                }
            }

            Common.IconMenuItem {
                text: "設定ダイアログの表示"
                iconName: "equalizer_line"
                onTriggered: {
                    if (WindowManager)
                        WindowManager.objectSettingsVisible = true;

                }
            }

        }

        Menu {
            title: "その他"

            Common.IconMenuItem {
                text: "バージョン情報"
                iconName: "information_line"
                onTriggered: {
                    var win = WindowManager.getWindow("about");
                    if (win) {
                        win.x = mainWin.x + (mainWin.width - win.width) / 2;
                        win.y = mainWin.y + (mainWin.height - win.height) / 2;
                        win.show();
                        win.raise();
                        win.requestActivate();
                    }
                }
            }

        }

    }

}
