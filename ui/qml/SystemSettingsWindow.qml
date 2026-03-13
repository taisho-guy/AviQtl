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
    property var themeLabels: ["ダーク", "ライト", "システムに従う"]
    property var timeUnitValues: ["frame", "second"]
    property var timeUnitLabels: ["フレーム", "秒"]
    property var renderThreadValues: [0, 1, 2, 4, 8, 16]
    property var renderThreadLabels: ["自動", "1", "2", "4", "8", "16"]
    property var audioChannelValues: [1, 2]
    property var audioChannelLabels: ["モノラル", "ステレオ"]
    property var blockSizeValues: [256, 512, 1024, 2048, 4096, 8192]
    property var videoCodecValues: ["h264_vaapi", "hevc_vaapi", "libx264"]
    property var videoCodecLabels: ["H.264 (VAAPI)", "HEVC (VAAPI)", "H.264 (CPU)"]
    property var audioCodecValues: ["aac", "opus", "flac", "pcm_s16le"]
    property var audioCodecLabels: ["AAC", "Opus", "FLAC", "PCM 16bit"]

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
    title: "システム設定"
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
                text: "一般"
            }

            TabButton {
                text: "性能"
            }

            TabButton {
                text: "タイムライン"
            }

            TabButton {
                text: "外観"
            }

            TabButton {
                text: "新規プロジェクト"
            }

            TabButton {
                text: "書き出し"
            }

            TabButton {
                text: "デコードと音声"
            }

            TabButton {
                text: "プラグイン"
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
                        title: "ファイル"
                        Layout.fillWidth: true

                        ColumnLayout {
                            anchors.fill: parent

                            CheckBox {
                                text: "終了時に確認ダイアログを表示する"
                                checked: root.valueOr("showConfirmOnClose", true)
                                onToggled: root.setValue("showConfirmOnClose", checked)
                            }

                            CheckBox {
                                text: "自動バックアップを有効にする"
                                checked: root.valueOr("enableAutoBackup", true)
                                onToggled: root.setValue("enableAutoBackup", checked)
                            }

                            RowLayout {
                                enabled: root.valueOr("enableAutoBackup", true)

                                Label {
                                    text: "バックアップ間隔"
                                }

                                SpinBox {
                                    from: 1
                                    to: 60
                                    value: root.valueOr("backupInterval", 5)
                                    onValueModified: root.setValue("backupInterval", value)
                                }

                                Label {
                                    text: "分"
                                }

                            }

                            RowLayout {
                                Label {
                                    text: "最近使ったプロジェクトの保持数"
                                }

                                SpinBox {
                                    from: 1
                                    to: 50
                                    value: root.valueOr("recentProjectMaxCount", 10)
                                    onValueModified: root.setValue("recentProjectMaxCount", value)
                                }

                                Label {
                                    text: "件"
                                }

                            }

                        }

                    }

                    GroupBox {
                        title: "編集"
                        Layout.fillWidth: true

                        ColumnLayout {
                            anchors.fill: parent

                            RowLayout {
                                Label {
                                    text: "元に戻す回数"
                                }

                                SpinBox {
                                    from: 1
                                    to: 1000
                                    value: root.valueOr("undoCount", 32)
                                    onValueModified: root.setValue("undoCount", value)
                                }

                            }

                            Label {
                                text: "回数を増やすとメモリ使用量が増えます"
                                color: palette.mid
                                font.pixelSize: 11
                            }

                        }

                    }

                    GroupBox {
                        title: "起動"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: "スプラッシュ表示時間"
                            }

                            SpinBox {
                                from: 0
                                to: 10000
                                stepSize: 100
                                value: root.valueOr("splashDuration", 1000)
                                onValueModified: root.setValue("splashDuration", value)
                            }

                            Label {
                                text: "スプラッシュ画像サイズ"
                            }

                            SpinBox {
                                from: 128
                                to: 2048
                                stepSize: 64
                                value: root.valueOr("splashSize", 512)
                                onValueModified: root.setValue("splashSize", value)
                            }

                            Label {
                                text: "起動後の遅延時間"
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
                        title: "メモリとキャッシュ"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: "最大画像サイズ"
                            }

                            ComboBox {
                                model: ["1280x720", "1920x1080", "2560x1440", "3840x2160"]
                                currentIndex: Math.max(0, model.indexOf(root.valueOr("maxImageSize", "1920x1080")))
                                onActivated: root.setValue("maxImageSize", currentText)
                            }

                            Label {
                                text: "キャッシュ容量"
                            }

                            SpinBox {
                                from: 64
                                to: 8192
                                stepSize: 64
                                value: root.valueOr("cacheSize", 512)
                                onValueModified: root.setValue("cacheSize", value)
                            }

                            Label {
                                text: "描画スレッド数"
                            }

                            ComboBox {
                                model: root.renderThreadLabels
                                currentIndex: root.indexOfValue(root.renderThreadValues, root.valueOr("renderThreads", 0), 0)
                                onActivated: root.setValue("renderThreads", root.renderThreadValues[currentIndex])
                            }

                        }

                    }

                    GroupBox {
                        title: "補足"
                        Layout.fillWidth: true

                        ColumnLayout {
                            anchors.fill: parent

                            Label {
                                text: "描画スレッド数が自動のときは実行環境に応じて決定します"
                                wrapMode: Text.WordWrap
                                color: palette.mid
                            }

                            Label {
                                text: "CachyOS の最新環境では、まず自動設定のまま確認する構成が扱いやすいです"
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
                        title: "操作"
                        Layout.fillWidth: true

                        ColumnLayout {
                            anchors.fill: parent

                            RowLayout {
                                Label {
                                    text: "時間表示"
                                }

                                ComboBox {
                                    model: root.timeUnitLabels
                                    currentIndex: root.indexOfValue(root.timeUnitValues, root.valueOr("timeUnit", "frame"), 0)
                                    onActivated: root.setValue("timeUnit", root.timeUnitValues[currentIndex])
                                }

                            }

                            CheckBox {
                                text: "スナップを有効にする"
                                checked: root.valueOr("enableSnap", true)
                                onToggled: root.setValue("enableSnap", checked)
                            }

                            CheckBox {
                                text: "分割時にカーソル位置を使う"
                                checked: root.valueOr("splitAtCursor", true)
                                onToggled: root.setValue("splitAtCursor", checked)
                            }

                            CheckBox {
                                text: "レイヤー範囲を表示する"
                                checked: root.valueOr("showLayerRange", true)
                                onToggled: root.setValue("showLayerRange", checked)
                            }

                        }

                    }

                    GroupBox {
                        title: "見た目と寸法"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: "トラックの高さ"
                            }

                            SpinBox {
                                from: 16
                                to: 100
                                value: root.valueOr("timelineTrackHeight", 30)
                                onValueModified: root.setValue("timelineTrackHeight", value)
                            }

                            Label {
                                text: "ヘッダーの高さ"
                            }

                            SpinBox {
                                from: 16
                                to: 100
                                value: root.valueOr("timelineHeaderHeight", 28)
                                onValueModified: root.setValue("timelineHeaderHeight", value)
                            }

                            Label {
                                text: "ルーラーの高さ"
                            }

                            SpinBox {
                                from: 16
                                to: 100
                                value: root.valueOr("timelineRulerHeight", 32)
                                onValueModified: root.setValue("timelineRulerHeight", value)
                            }

                            Label {
                                text: "最大レイヤー数"
                            }

                            SpinBox {
                                from: 1
                                to: 512
                                value: root.valueOr("timelineMaxLayers", 128)
                                onValueModified: root.setValue("timelineMaxLayers", value)
                            }

                            Label {
                                text: "レイヤーヘッダー幅"
                            }

                            SpinBox {
                                from: 40
                                to: 300
                                value: root.valueOr("timelineLayerHeaderWidth", 60)
                                onValueModified: root.setValue("timelineLayerHeaderWidth", value)
                            }

                            Label {
                                text: "時間表示欄の幅"
                            }

                            SpinBox {
                                from: 40
                                to: 300
                                value: root.valueOr("timelineRulerTimeWidth", 70)
                                onValueModified: root.setValue("timelineRulerTimeWidth", value)
                            }

                            Label {
                                text: "クリップ端のつかみ幅"
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
                        title: "編集制約とズーム"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: "最小クリップ長"
                            }

                            SpinBox {
                                from: 1
                                to: 100
                                value: root.valueOr("minClipDurationFrames", 5)
                                onValueModified: root.setValue("minClipDurationFrames", value)
                            }

                            Label {
                                text: "磁力スナップ範囲"
                            }

                            SpinBox {
                                from: 1
                                to: 100
                                value: root.valueOr("magneticSnapRange", 10)
                                onValueModified: root.setValue("magneticSnapRange", value)
                            }

                            Label {
                                text: "ズーム最小値"
                            }

                            SpinBox {
                                from: 1
                                to: 1000
                                value: root.valueOr("timelineZoomMin", 10)
                                onValueModified: root.setValue("timelineZoomMin", value)
                            }

                            Label {
                                text: "ズーム最大値"
                            }

                            SpinBox {
                                from: 1
                                to: 4000
                                value: root.valueOr("timelineZoomMax", 400)
                                onValueModified: root.setValue("timelineZoomMax", value)
                            }

                            Label {
                                text: "ズーム刻み"
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
                        title: "表示"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: "テーマ"
                            }

                            ComboBox {
                                model: root.themeLabels
                                currentIndex: root.indexOfValue(root.themeValues, root.valueOr("theme", "Dark"), 0)
                                onActivated: root.setValue("theme", root.themeValues[currentIndex])
                            }

                            Label {
                                text: "文字余白係数"
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
                        text: "テーマ変更は再起動後に完全反映される場合があります"
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
                        title: "既定値"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: "幅"
                            }

                            SpinBox {
                                from: 1
                                to: 16000
                                value: root.valueOr("defaultProjectWidth", 1920)
                                onValueModified: root.setValue("defaultProjectWidth", value)
                            }

                            Label {
                                text: "高さ"
                            }

                            SpinBox {
                                from: 1
                                to: 16000
                                value: root.valueOr("defaultProjectHeight", 1080)
                                onValueModified: root.setValue("defaultProjectHeight", value)
                            }

                            Label {
                                text: "フレームレート"
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
                                text: "総フレーム数"
                            }

                            SpinBox {
                                from: 1
                                to: 1e+06
                                value: root.valueOr("defaultProjectFrames", 3600)
                                onValueModified: root.setValue("defaultProjectFrames", value)
                            }

                            Label {
                                text: "サンプリング周波数"
                            }

                            SpinBox {
                                from: 8000
                                to: 192000
                                stepSize: 1000
                                value: root.valueOr("defaultProjectSampleRate", 48000)
                                onValueModified: root.setValue("defaultProjectSampleRate", value)
                            }

                            Label {
                                text: "既定クリップ長"
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
                        title: "映像"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: "既定の映像コーデック"
                            }

                            ComboBox {
                                model: root.videoCodecLabels
                                currentIndex: root.indexOfValue(root.videoCodecValues, root.valueOr("exportDefaultCodec", "h264_vaapi"), 0)
                                onActivated: root.setValue("exportDefaultCodec", root.videoCodecValues[currentIndex])
                            }

                            Label {
                                text: "既定ビットレート"
                            }

                            SpinBox {
                                from: 1
                                to: 500
                                value: root.valueOr("exportDefaultBitrateMbps", 15)
                                onValueModified: root.setValue("exportDefaultBitrateMbps", value)
                            }

                            Label {
                                text: "既定CRF"
                            }

                            SpinBox {
                                from: 0
                                to: 51
                                value: root.valueOr("exportDefaultCrf", 20)
                                onValueModified: root.setValue("exportDefaultCrf", value)
                            }

                            Label {
                                text: "静止画品質"
                            }

                            SpinBox {
                                from: 0
                                to: 100
                                value: root.valueOr("exportImageQuality", 95)
                                onValueModified: root.setValue("exportImageQuality", value)
                            }

                            Label {
                                text: "連番桁数"
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
                        title: "音声と進行表示"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: "既定の音声コーデック"
                            }

                            ComboBox {
                                model: root.audioCodecLabels
                                currentIndex: root.indexOfValue(root.audioCodecValues, root.valueOr("exportDefaultAudioCodec", "aac"), 0)
                                onActivated: root.setValue("exportDefaultAudioCodec", root.audioCodecValues[currentIndex])
                            }

                            Label {
                                text: "音声ビットレート"
                            }

                            SpinBox {
                                from: 32
                                to: 1536
                                stepSize: 32
                                value: root.valueOr("exportDefaultAudioBitrateKbps", 192)
                                onValueModified: root.setValue("exportDefaultAudioBitrateKbps", value)
                            }

                            Label {
                                text: "フレーム取得待ち時間"
                            }

                            SpinBox {
                                from: 100
                                to: 10000
                                stepSize: 100
                                value: root.valueOr("exportFrameGrabTimeoutMs", 2000)
                                onValueModified: root.setValue("exportFrameGrabTimeoutMs", value)
                            }

                            Label {
                                text: "進捗更新間隔"
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
                        title: "映像デコード"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: "インデックス予約数"
                            }

                            SpinBox {
                                from: 1000
                                to: 1e+06
                                stepSize: 1000
                                value: root.valueOr("videoDecoderIndexReserve", 108000)
                                onValueModified: root.setValue("videoDecoderIndexReserve", value)
                            }

                            Label {
                                text: "最小キャッシュ量"
                            }

                            SpinBox {
                                from: 16
                                to: 4096
                                stepSize: 16
                                value: root.valueOr("videoDecoderMinCacheMB", 64)
                                onValueModified: root.setValue("videoDecoderMinCacheMB", value)
                            }

                            Label {
                                text: "ハードウェアフレームプール数"
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
                        title: "音声"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            anchors.fill: parent

                            Label {
                                text: "音声チャンネル数"
                            }

                            ComboBox {
                                model: root.audioChannelLabels
                                currentIndex: root.indexOfValue(root.audioChannelValues, root.valueOr("audioChannels", 2), 1)
                                onActivated: root.setValue("audioChannels", root.audioChannelValues[currentIndex])
                            }

                            Label {
                                text: "プラグイン最大ブロックサイズ"
                            }

                            ComboBox {
                                model: root.blockSizeValues
                                currentIndex: root.indexOfValue(root.blockSizeValues, root.valueOr("audioPluginMaxBlockSize", 4096), 4)
                                onActivated: root.setValue("audioPluginMaxBlockSize", root.blockSizeValues[currentIndex])
                            }

                            Label {
                                text: "Lua フック間隔"
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
                        text: "デコードと音声関連の設定は再起動後に効果が分かりやすい項目を含みます"
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
                        text: "各形式ごとに有効化と検索パスを設定できます"
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
                                    text: modelData + " を読み込む"
                                    checked: root.valueOr("pluginEnable" + modelData, true)
                                    onToggled: root.setPluginEnabled(modelData, checked)
                                }

                                Label {
                                    text: "検索パス"
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
                                    text: "1行に1パスを入力します"
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

        }

        Frame {
            Layout.fillWidth: true
            padding: 8

            RowLayout {
                anchors.fill: parent
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: "設定は「適用」または「OK」で保存されます"
                    color: palette.mid
                    elide: Text.ElideRight
                }

                Button {
                    text: "再読込"
                    onClicked: root.loadSettings()
                }

                Button {
                    text: "適用"
                    onClicked: root.applySettings()
                }

                Button {
                    text: "OK"
                    highlighted: true
                    onClicked: {
                        root.applySettings();
                        root.hide();
                    }
                }

                Button {
                    text: "閉じる"
                    onClicked: root.hide()
                }

            }

        }

    }

}
