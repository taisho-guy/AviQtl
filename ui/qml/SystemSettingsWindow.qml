import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "common" as Common

Common.RinaWindow {
    id: root

    property var draftSettings: ({
    })
    property alias currentTabIndex: tabBar.currentIndex
    property var pluginFormats: ["LADSPA", "DSSI", "LV2", "VST2", "VST3", "CLAP", "SF2", "SFZ", "JSFX"]
    property var themeValues: ["Dark", "Light", "System"]
    property var themeLabels: [qsTr("ダーク"), qsTr("ライト"), qsTr("システムに従う")]
    property var timeUnitValues: ["frame", "second"]
    property var timeUnitLabels: [qsTr("フレーム"), qsTr("秒")]
    property var renderThreadValues: [0, 1, 2, 4, 8, 16]
    property var renderThreadLabels: [qsTr("自動"), "1", "2", "4", "8", "16"]
    property var audioChannelValues: [1, 2]
    property var audioChannelLabels: [qsTr("モノラル"), qsTr("ステレオ")]
    property var blockSizeValues: [256, 512, 1024, 2048, 4096, 8192]
    property var videoCodecValues: ["h264_vaapi", "hevc_vaapi", "libx264"]
    property var videoCodecLabels: [qsTr("H.264 (VAAPI)"), qsTr("HEVC (VAAPI)"), qsTr("H.264 (CPU)")]
    property var audioCodecValues: ["aac", "opus", "flac", "pcm_s16le"]
    property var audioCodecLabels: ["AAC", "Opus", "FLAC", "PCM 16bit"]
    property var shortcutList: [{
        "id": "project.new",
        "name": qsTr("新規プロジェクト")
    }, {
        "id": "project.open",
        "name": qsTr("プロジェクトを開く")
    }, {
        "id": "project.save",
        "name": qsTr("上書き保存")
    }, {
        "id": "project.saveAs",
        "name": qsTr("名前を付けて保存")
    }, {
        "id": "app.quit",
        "name": qsTr("終了")
    }, {
        "id": "edit.undo",
        "name": qsTr("元に戻す")
    }, {
        "id": "edit.redo",
        "name": qsTr("やり直す")
    }, {
        "id": "edit.cut",
        "name": qsTr("カット")
    }, {
        "id": "edit.copy",
        "name": qsTr("コピー")
    }, {
        "id": "edit.paste",
        "name": qsTr("貼り付け")
    }, {
        "id": "edit.delete",
        "name": qsTr("削除")
    }, {
        "id": "edit.duplicate",
        "name": qsTr("複製")
    }, {
        "id": "transport.playPause",
        "name": qsTr("再生 / 一時停止")
    }, {
        "id": "transport.nextFrame",
        "name": qsTr("1フレーム進む")
    }, {
        "id": "transport.prevFrame",
        "name": qsTr("1フレーム戻る")
    }, {
        "id": "transport.jumpStart",
        "name": qsTr("先頭へ移動")
    }, {
        "id": "transport.jumpEnd",
        "name": qsTr("末尾へ移動")
    }, {
        "id": "view.zoomIn",
        "name": qsTr("ズームイン")
    }, {
        "id": "view.zoomOut",
        "name": qsTr("ズームアウト")
    }, {
        "id": "timeline.split",
        "name": qsTr("クリップを分割")
    }, {
        "id": "timeline.moveUp",
        "name": qsTr("レイヤーを上へ移動")
    }, {
        "id": "timeline.moveDown",
        "name": qsTr("レイヤーを下へ移動")
    }, {
        "id": "timeline.nudgeLeft",
        "name": qsTr("1フレーム左へ移動")
    }, {
        "id": "timeline.nudgeRight",
        "name": qsTr("1フレーム右へ移動")
    }]

    function getShortcutValue(actionId, fallback) {
        if (!draftSettings["shortcuts"])
            return fallback;

        return draftSettings["shortcuts"][actionId] !== undefined ? draftSettings["shortcuts"][actionId] : fallback;
    }

    function setShortcutValue(actionId, value) {
        var next = cloneSettings(draftSettings);
        if (!next["shortcuts"])
            next["shortcuts"] = {
        };

        next["shortcuts"][actionId] = value;
        draftSettings = next;
    }

    function cloneSettings(source) {
        return JSON.parse(JSON.stringify(source || {
        }));
    }

    function loadSettings() {
        if (SettingsManager)
            draftSettings = cloneSettings(SettingsManager.settings);
        else
            draftSettings = ({
        });
    }

    function applySettings() {
        if (!SettingsManager)
            return ;

        SettingsManager.settings = cloneSettings(draftSettings);
        SettingsManager.save();
    }

    function valueOr(key, fallbackValue) {
        return draftSettings[key] !== undefined ? draftSettings[key] : fallbackValue;
    }

    function setValue(key, value) {
        var next = cloneSettings(draftSettings);
        next[key] = value;
        draftSettings = next;
    }

    function indexOfValue(values, targetValue, fallbackIndex) {
        for (var i = 0; i < values.length; ++i) {
            if (values[i] === targetValue)
                return i;

        }
        return fallbackIndex;
    }

    function pluginPathsText(formatName) {
        var values = draftSettings["pluginPaths" + formatName];
        if (!values)
            return "";

        return values.join("\n");
    }

    function setPluginEnabled(formatName, enabled) {
        setValue("pluginEnable" + formatName, enabled);
    }

    function setPluginPathsFromText(formatName, textValue) {
        var lines = textValue.split(/\r?\n/);
        var paths = [];
        for (var i = 0; i < lines.length; ++i) {
            var path = lines[i].trim();
            if (path.length > 0)
                paths.push(path);

        }
        setValue("pluginPaths" + formatName, paths);
    }

    width: 760
    height: 680
    title: qsTr("システム設定")
    Component.onCompleted: loadSettings()
    onVisibleChanged: {
        if (visible)
            loadSettings();

    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        TabBar {
            id: tabBar

            Layout.fillWidth: true

            TabButton {
                text: qsTr("一般")
            }

            TabButton {
                text: qsTr("性能")
            }

            TabButton {
                text: qsTr("タイムライン")
            }

            TabButton {
                text: qsTr("外観")
            }

            TabButton {
                text: qsTr("新規プロジェクト")
            }

            TabButton {
                text: qsTr("書き出し")
            }

            TabButton {
                text: qsTr("デコードと音声")
            }

            TabButton {
                text: qsTr("プラグイン")
            }

            TabButton {
                text: qsTr("ショートカット")
            }

        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            ScrollView {
                id: generalPage

                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: generalPage.availableWidth
                    spacing: 14

                    GroupBox {
                        title: qsTr("ファイル")
                        Layout.fillWidth: true

                        ColumnLayout {
                            anchors.fill: parent

                            CheckBox {
                                text: qsTr("終了時に確認ダイアログを表示する")
                                checked: root.valueOr("showConfirmOnClose", true)
                                onToggled: root.setValue("showConfirmOnClose", checked)
                            }

                            CheckBox {
                                text: qsTr("自動バックアップを有効にする")
                                checked: root.valueOr("enableAutoBackup", true)
                                onToggled: root.setValue("enableAutoBackup", checked)
                            }

                            RowLayout {
                                enabled: root.valueOr("enableAutoBackup", true)

                                Label {
                                    text: qsTr("バックアップ間隔")
                                }

                                SpinBox {
                                    from: 1
                                    to: 60
                                    value: root.valueOr("backupInterval", 5)
                                    onValueModified: root.setValue("backupInterval", value)
                                }

                                Label {
                                    text: qsTr("分")
                                }

                            }

                            RowLayout {
                                Label {
                                    text: qsTr("最近使ったプロジェクトの保持数")
                                }

                                SpinBox {
                                    from: 1
                                    to: 50
                                    value: root.valueOr("recentProjectMaxCount", 10)
                                    onValueModified: root.setValue("recentProjectMaxCount", value)
                                }

                                Label {
                                    text: qsTr("件")
                                }

                            }

                        }

                    }

                    GroupBox {
                        title: qsTr("編集")
                        Layout.fillWidth: true

                        ColumnLayout {
                            anchors.fill: parent

                            RowLayout {
                                Label {
                                    text: qsTr("元に戻す回数")
                                }

                                SpinBox {
                                    from: 1
                                    to: 1000
                                    value: root.valueOr("undoCount", 32)
                                    onValueModified: root.setValue("undoCount", value)
                                }

                            }

                            Label {
                                text: qsTr("回数を増やすとメモリ使用量が増えます")
                                color: palette.mid
                                font.pixelSize: 11
                            }

                        }

                    }

                    GroupBox {
                        title: qsTr("起動")
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: qsTr("スプラッシュ表示時間")
                            }

                            SpinBox {
                                from: 0
                                to: 10000
                                stepSize: 100
                                value: root.valueOr("splashDuration", 512)
                                onValueModified: root.setValue("splashDuration", value)
                            }

                            Label {
                                text: qsTr("スプラッシュ画像サイズ")
                            }

                            SpinBox {
                                from: 128
                                to: 2048
                                stepSize: 64
                                value: root.valueOr("splashSize", 128)
                                onValueModified: root.setValue("splashSize", value)
                            }

                            Label {
                                text: qsTr("起動後の遅延時間")
                            }

                            SpinBox {
                                from: 0
                                to: 10000
                                stepSize: 100
                                value: root.valueOr("appStartupDelay", 1000)
                                onValueModified: root.setValue("appStartupDelay", value)
                            }

                        }

                    }

                    Item {
                        Layout.fillHeight: true
                    }

                }

            }

            ScrollView {
                id: performancePage

                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: performancePage.availableWidth
                    spacing: 14

                    GroupBox {
                        title: qsTr("メモリとキャッシュ")
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: qsTr("最大画像サイズ")
                            }

                            ComboBox {
                                model: ["1280x720", "1920x1080", "2560x1440", "3840x2160"]
                                currentIndex: Math.max(0, model.indexOf(root.valueOr("maxImageSize", "1920x1080")))
                                onActivated: root.setValue("maxImageSize", currentText)
                            }

                            Label {
                                text: qsTr("キャッシュ容量")
                            }

                            SpinBox {
                                from: 64
                                to: 8192
                                stepSize: 64
                                value: root.valueOr("cacheSize", 512)
                                onValueModified: root.setValue("cacheSize", value)
                            }

                            Label {
                                text: qsTr("描画スレッド数")
                            }

                            ComboBox {
                                model: root.renderThreadLabels
                                currentIndex: root.indexOfValue(root.renderThreadValues, root.valueOr("renderThreads", 0), 0)
                                onActivated: root.setValue("renderThreads", root.renderThreadValues[currentIndex])
                            }

                        }

                    }

                    GroupBox {
                        title: qsTr("補足")
                        Layout.fillWidth: true

                        ColumnLayout {
                            anchors.fill: parent

                            Label {
                                text: qsTr("描画スレッド数が自動のときは実行環境に応じて決定します")
                                wrapMode: Text.WordWrap
                                color: palette.mid
                            }

                            Label {
                                text: qsTr("ご使用の実行環境に合わせて、まずは自動設定で動作を確認してください")
                                wrapMode: Text.WordWrap
                                color: palette.mid
                            }

                        }

                    }

                    Item {
                        Layout.fillHeight: true
                    }

                }

            }

            ScrollView {
                id: timelinePage

                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: timelinePage.availableWidth
                    spacing: 14

                    GroupBox {
                        title: qsTr("操作")
                        Layout.fillWidth: true

                        ColumnLayout {
                            anchors.fill: parent

                            RowLayout {
                                Label {
                                    text: qsTr("時間表示")
                                }

                                ComboBox {
                                    model: root.timeUnitLabels
                                    currentIndex: root.indexOfValue(root.timeUnitValues, root.valueOr("timeUnit", "frame"), 0)
                                    onActivated: root.setValue("timeUnit", root.timeUnitValues[currentIndex])
                                }

                            }

                            CheckBox {
                                text: qsTr("分割時にカーソル位置を使う")
                                checked: root.valueOr("splitAtCursor", true)
                                onToggled: root.setValue("splitAtCursor", checked)
                            }

                            CheckBox {
                                text: qsTr("レイヤー範囲を表示する")
                                checked: root.valueOr("showLayerRange", true)
                                onToggled: root.setValue("showLayerRange", checked)
                            }

                        }

                    }

                    GroupBox {
                        title: qsTr("見た目と寸法")
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: qsTr("トラックの高さ")
                            }

                            SpinBox {
                                from: 16
                                to: 100
                                value: root.valueOr("timelineTrackHeight", 30)
                                onValueModified: root.setValue("timelineTrackHeight", value)
                            }

                            Label {
                                text: qsTr("ヘッダーの高さ")
                            }

                            SpinBox {
                                from: 16
                                to: 100
                                value: root.valueOr("timelineHeaderHeight", 28)
                                onValueModified: root.setValue("timelineHeaderHeight", value)
                            }

                            Label {
                                text: qsTr("設定サイドバーを右に配置")
                            }

                            CheckBox {
                                checked: root.valueOr("settingDialogSidebarRight", false)
                                onToggled: root.setValue("settingDialogSidebarRight", checked)
                            }

                            Label {
                                text: qsTr("ルーラーの高さ")
                            }

                            SpinBox {
                                from: 16
                                to: 100
                                value: root.valueOr("timelineRulerHeight", 32)
                                onValueModified: root.setValue("timelineRulerHeight", value)
                            }

                            Label {
                                text: qsTr("最大レイヤー数")
                            }

                            SpinBox {
                                from: 1
                                to: 512
                                value: root.valueOr("timelineMaxLayers", 128)
                                onValueModified: root.setValue("timelineMaxLayers", value)
                            }

                            Label {
                                text: qsTr("レイヤーヘッダー幅")
                            }

                            SpinBox {
                                from: 40
                                to: 300
                                value: root.valueOr("timelineLayerHeaderWidth", 60)
                                onValueModified: root.setValue("timelineLayerHeaderWidth", value)
                            }

                            Label {
                                text: qsTr("時間表示欄の幅")
                            }

                            SpinBox {
                                from: 40
                                to: 300
                                value: root.valueOr("timelineRulerTimeWidth", 70)
                                onValueModified: root.setValue("timelineRulerTimeWidth", value)
                            }

                            Label {
                                text: qsTr("クリップ端のつかみ幅")
                            }

                            SpinBox {
                                from: 4
                                to: 40
                                value: root.valueOr("timelineClipResizeHandleWidth", 10)
                                onValueModified: root.setValue("timelineClipResizeHandleWidth", value)
                            }

                        }

                    }

                    GroupBox {
                        title: qsTr("編集制約とズーム")
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: qsTr("最小クリップ長")
                            }

                            SpinBox {
                                from: 1
                                to: 100
                                value: root.valueOr("minClipDurationFrames", 5)
                                onValueModified: root.setValue("minClipDurationFrames", value)
                            }

                            Label {
                                text: qsTr("ズーム最小値")
                            }

                            SpinBox {
                                from: 1
                                to: 1000
                                value: root.valueOr("timelineZoomMin", 10)
                                onValueModified: root.setValue("timelineZoomMin", value)
                            }

                            Label {
                                text: qsTr("ズーム最大値")
                            }

                            SpinBox {
                                from: 1
                                to: 4000
                                value: root.valueOr("timelineZoomMax", 400)
                                onValueModified: root.setValue("timelineZoomMax", value)
                            }

                            Label {
                                text: qsTr("ズーム刻み")
                            }

                            SpinBox {
                                from: 1
                                to: 100
                                value: root.valueOr("timelineZoomStep", 10)
                                onValueModified: root.setValue("timelineZoomStep", value)
                            }

                        }

                    }

                    Item {
                        Layout.fillHeight: true
                    }

                }

            }

            ScrollView {
                id: appearancePage

                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: appearancePage.availableWidth
                    spacing: 14

                    GroupBox {
                        title: qsTr("表示")
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: qsTr("テーマ")
                            }

                            ComboBox {
                                id: themeComboBox

                                model: typeof ColorSchemeController !== "undefined" ? ColorSchemeController.schemesModel : null
                                textRole: "display"
                                Component.onCompleted: {
                                    if (typeof ColorSchemeController !== "undefined") {
                                        var idx = ColorSchemeController.indexOfSchemeId(ColorSchemeController.activeSchemeId);
                                        if (idx !== -1)
                                            currentIndex = idx;

                                    }
                                }
                                onActivated: {
                                    if (typeof ColorSchemeController !== "undefined")
                                        ColorSchemeController.activeSchemeId = ColorSchemeController.schemeIdAt(currentIndex);

                                }
                            }

                            Label {
                                text: qsTr("文字余白係数")
                            }

                            SpinBox {
                                from: 1
                                to: 20
                                stepSize: 1
                                value: Math.round(root.valueOr("textPaddingMultiplier", 4) * 10)
                                textFromValue: function(value, locale) {
                                    return (value / 10).toFixed(1);
                                }
                                valueFromText: function(text, locale) {
                                    return Math.round(Number(text) * 10);
                                }
                                onValueModified: root.setValue("textPaddingMultiplier", value / 10)
                            }

                        }

                    }

                    Label {
                        text: qsTr("テーマ変更は再起動後に完全反映される場合があります")
                        color: palette.mid
                        wrapMode: Text.WordWrap
                    }

                    Item {
                        Layout.fillHeight: true
                    }

                }

            }

            ScrollView {
                id: projectPage

                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: projectPage.availableWidth
                    spacing: 14

                    GroupBox {
                        title: qsTr("既定値")
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: qsTr("幅")
                            }

                            SpinBox {
                                from: 1
                                to: 16000
                                value: root.valueOr("defaultProjectWidth", 1920)
                                onValueModified: root.setValue("defaultProjectWidth", value)
                            }

                            Label {
                                text: qsTr("高さ")
                            }

                            SpinBox {
                                from: 1
                                to: 16000
                                value: root.valueOr("defaultProjectHeight", 1080)
                                onValueModified: root.setValue("defaultProjectHeight", value)
                            }

                            Label {
                                text: qsTr("フレームレート")
                            }

                            SpinBox {
                                from: 100
                                to: 24000
                                stepSize: 100
                                value: Math.round(root.valueOr("defaultProjectFps", 60) * 100)
                                textFromValue: function(value, locale) {
                                    return (value / 100).toFixed(2);
                                }
                                valueFromText: function(text, locale) {
                                    return Math.round(Number(text) * 100);
                                }
                                onValueModified: root.setValue("defaultProjectFps", value / 100)
                            }

                            Label {
                                text: qsTr("総フレーム数")
                            }

                            SpinBox {
                                from: 1
                                to: 1e+06
                                value: root.valueOr("defaultProjectFrames", 3600)
                                onValueModified: root.setValue("defaultProjectFrames", value)
                            }

                            Label {
                                text: qsTr("サンプリング周波数")
                            }

                            SpinBox {
                                from: 8000
                                to: 192000
                                stepSize: 1000
                                value: root.valueOr("defaultProjectSampleRate", 48000)
                                onValueModified: root.setValue("defaultProjectSampleRate", value)
                            }

                            Label {
                                text: qsTr("既定クリップ長")
                            }

                            SpinBox {
                                from: 1
                                to: 100000
                                value: root.valueOr("defaultClipDuration", 100)
                                onValueModified: root.setValue("defaultClipDuration", value)
                            }

                        }

                    }

                    Item {
                        Layout.fillHeight: true
                    }

                }

            }

            ScrollView {
                id: exportPage

                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: exportPage.availableWidth
                    spacing: 14

                    GroupBox {
                        title: qsTr("映像")
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: qsTr("既定の映像コーデック")
                            }

                            ComboBox {
                                model: root.videoCodecLabels
                                currentIndex: root.indexOfValue(root.videoCodecValues, root.valueOr("exportDefaultCodec", "h264_vaapi"), 0)
                                onActivated: root.setValue("exportDefaultCodec", root.videoCodecValues[currentIndex])
                            }

                            Label {
                                text: qsTr("既定ビットレート")
                            }

                            SpinBox {
                                from: 1
                                to: 500
                                value: root.valueOr("exportDefaultBitrateMbps", 15)
                                onValueModified: root.setValue("exportDefaultBitrateMbps", value)
                            }

                            Label {
                                text: qsTr("既定CRF")
                            }

                            SpinBox {
                                from: 0
                                to: 51
                                value: root.valueOr("exportDefaultCrf", 20)
                                onValueModified: root.setValue("exportDefaultCrf", value)
                            }

                            Label {
                                text: qsTr("静止画品質")
                            }

                            SpinBox {
                                from: 0
                                to: 100
                                value: root.valueOr("exportImageQuality", 95)
                                onValueModified: root.setValue("exportImageQuality", value)
                            }

                            Label {
                                text: qsTr("連番桁数")
                            }

                            SpinBox {
                                from: 2
                                to: 10
                                value: root.valueOr("exportSequencePadding", 6)
                                onValueModified: root.setValue("exportSequencePadding", value)
                            }

                        }

                    }

                    GroupBox {
                        title: qsTr("音声と進行表示")
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: qsTr("既定の音声コーデック")
                            }

                            ComboBox {
                                model: root.audioCodecLabels
                                currentIndex: root.indexOfValue(root.audioCodecValues, root.valueOr("exportDefaultAudioCodec", "aac"), 0)
                                onActivated: root.setValue("exportDefaultAudioCodec", root.audioCodecValues[currentIndex])
                            }

                            Label {
                                text: qsTr("音声ビットレート")
                            }

                            SpinBox {
                                from: 32
                                to: 1536
                                stepSize: 32
                                value: root.valueOr("exportDefaultAudioBitrateKbps", 192)
                                onValueModified: root.setValue("exportDefaultAudioBitrateKbps", value)
                            }

                            Label {
                                text: qsTr("フレーム取得待ち時間")
                            }

                            SpinBox {
                                from: 100
                                to: 10000
                                stepSize: 100
                                value: root.valueOr("exportFrameGrabTimeoutMs", 2000)
                                onValueModified: root.setValue("exportFrameGrabTimeoutMs", value)
                            }

                            Label {
                                text: qsTr("進捗更新間隔")
                            }

                            SpinBox {
                                from: 1
                                to: 60
                                value: root.valueOr("exportProgressInterval", 5)
                                onValueModified: root.setValue("exportProgressInterval", value)
                            }

                        }

                    }

                    Item {
                        Layout.fillHeight: true
                    }

                }

            }

            ScrollView {
                id: decodeAudioPage

                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: decodeAudioPage.availableWidth
                    spacing: 14

                    GroupBox {
                        title: qsTr("映像デコード")
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: qsTr("インデックス予約数")
                            }

                            SpinBox {
                                from: 1000
                                to: 1e+06
                                stepSize: 1000
                                value: root.valueOr("videoDecoderIndexReserve", 108000)
                                onValueModified: root.setValue("videoDecoderIndexReserve", value)
                            }

                            Label {
                                text: qsTr("最小キャッシュ量")
                            }

                            SpinBox {
                                from: 16
                                to: 4096
                                stepSize: 16
                                value: root.valueOr("videoDecoderMinCacheMB", 64)
                                onValueModified: root.setValue("videoDecoderMinCacheMB", value)
                            }

                            Label {
                                text: qsTr("ハードウェアフレームプール数")
                            }

                            SpinBox {
                                from: 1
                                to: 256
                                value: root.valueOr("hwFramePoolSize", 32)
                                onValueModified: root.setValue("hwFramePoolSize", value)
                            }

                        }

                    }

                    GroupBox {
                        title: qsTr("音声")
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: qsTr("音声チャンネル数")
                            }

                            ComboBox {
                                model: root.audioChannelLabels
                                currentIndex: root.indexOfValue(root.audioChannelValues, root.valueOr("audioChannels", 2), 1)
                                onActivated: root.setValue("audioChannels", root.audioChannelValues[currentIndex])
                            }

                            Label {
                                text: qsTr("プラグイン最大ブロックサイズ")
                            }

                            ComboBox {
                                model: root.blockSizeValues
                                currentIndex: root.indexOfValue(root.blockSizeValues, root.valueOr("audioPluginMaxBlockSize", 4096), 4)
                                onActivated: root.setValue("audioPluginMaxBlockSize", root.blockSizeValues[currentIndex])
                            }

                            Label {
                                text: qsTr("Lua フック間隔")
                            }

                            SpinBox {
                                from: 1
                                to: 1000
                                stepSize: 1
                                value: root.valueOr("luaHookIntervalMs", 16)
                                onValueModified: root.setValue("luaHookIntervalMs", value)
                            }

                        }

                    }

                    Label {
                        text: qsTr("デコードと音声関連の設定は再起動後に反映されます")
                        color: palette.mid
                        wrapMode: Text.WordWrap
                    }

                    Item {
                        Layout.fillHeight: true
                    }

                }

            }

            ScrollView {
                id: pluginPage

                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: pluginPage.availableWidth
                    spacing: 14

                    Label {
                        text: qsTr("各形式ごとに有効化と検索パスを設定できます")
                        color: palette.mid
                        wrapMode: Text.WordWrap
                    }

                    Repeater {
                        model: root.pluginFormats

                        delegate: GroupBox {
                            required property string modelData

                            title: modelData
                            Layout.fillWidth: true

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 8

                                CheckBox {
                                    text: qsTr("%1 を読み込む").arg(modelData)
                                    checked: root.valueOr("pluginEnable" + modelData, true)
                                    onToggled: root.setPluginEnabled(modelData, checked)
                                }

                                Label {
                                    text: qsTr("検索パス")
                                    color: palette.mid
                                }

                                TextArea {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 96
                                    wrapMode: TextArea.NoWrap
                                    selectByMouse: true
                                    text: root.pluginPathsText(modelData)
                                    onActiveFocusChanged: {
                                        if (!activeFocus)
                                            root.setPluginPathsFromText(modelData, text);

                                    }

                                    background: Rectangle {
                                        color: palette.base
                                        border.color: palette.mid
                                        radius: 4
                                    }

                                }

                                Label {
                                    text: qsTr("1行に1パスを入力します")
                                    color: palette.mid
                                    font.pixelSize: 11
                                }

                            }

                        }

                    }

                    Item {
                        Layout.fillHeight: true
                    }

                }

            }

            ScrollView {
                id: shortcutPage

                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: shortcutPage.availableWidth
                    spacing: 14

                    Label {
                        text: qsTr("キーボードショートカット")
                        font.bold: true
                        font.pixelSize: 16
                    }

                    Label {
                        text: qsTr("「Ctrl+S」や「Alt+Shift+N」の形式で入力してください")
                        color: palette.mid
                    }

                    GridLayout {
                        columns: 2
                        Layout.fillWidth: true
                        columnSpacing: 16
                        rowSpacing: 10

                        Repeater {
                            model: root.shortcutList

                            delegate: RowLayout {
                                Layout.columnSpan: 2
                                Layout.fillWidth: true

                                Label {
                                    text: modelData.name
                                    Layout.preferredWidth: 150
                                }

                                TextField {
                                    id: shortcutInput

                                    Layout.fillWidth: true
                                    text: root.getShortcutValue(modelData.id, "")
                                    placeholderText: qsTr("未設定")
                                    onEditingFinished: {
                                        root.setShortcutValue(modelData.id, text);
                                    }
                                }

                            }

                        }

                    }

                    Item {
                        Layout.fillHeight: true
                    }

                }

            }

        }

        Frame {
            Layout.fillWidth: true
            padding: 8

            RowLayout {
                anchors.fill: parent
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: qsTr("設定は「適用」または「OK」で保存されます")
                    color: palette.mid
                    elide: Text.ElideRight
                }

                Button {
                    text: qsTr("再読込")
                    onClicked: root.loadSettings()
                }

                Button {
                    text: qsTr("適用")
                    onClicked: root.applySettings()
                }

                Button {
                    text: qsTr("OK")
                    highlighted: true
                    onClicked: {
                        root.applySettings();
                        root.hide();
                    }
                }

                Button {
                    text: qsTr("閉じる")
                    onClicked: root.hide()
                }

            }

        }

    }

}
