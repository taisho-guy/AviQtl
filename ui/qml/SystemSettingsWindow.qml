import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "common" as Common

Common.RinaWindow {
    id: root

    // C++のSettingsManagerとバインド
    property var settings: SettingsManager ? SettingsManager.settings : ({
    })
    property alias currentTabIndex: bar.currentIndex

    function set(key, val) {
        if (!SettingsManager)
            return ;

        var s = root.settings;
        s[key] = val;
        SettingsManager.settings = s;
    }

    width: 600
    height: 500
    title: "システムの設定"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        TabBar {
            id: bar

            Layout.fillWidth: true

            TabButton {
                text: "一般"
            }

            TabButton {
                text: "パフォーマンス"
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
                text: "エクスポート"
            }

            TabButton {
                text: "デコード / オーディオ"
            }

        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: bar.currentIndex

            // --- 一般設定 ---
            ScrollView {
                contentWidth: availableWidth

                ColumnLayout {
                    spacing: 15

                    GroupBox {
                        title: "ファイル"
                        Layout.fillWidth: true

                        ColumnLayout {
                            CheckBox {
                                text: "終了時に確認ダイアログを表示する"
                                checked: root.settings.showConfirmOnClose
                                onToggled: {
                                    var s = root.settings;
                                    s.showConfirmOnClose = checked;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                            CheckBox {
                                text: "自動バックアップを有効化"
                                checked: root.settings.enableAutoBackup
                                onToggled: {
                                    var s = root.settings;
                                    s.enableAutoBackup = checked;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                            RowLayout {
                                enabled: root.settings.enableAutoBackup

                                Label {
                                    text: "バックアップ間隔(分):"
                                }

                                SpinBox {
                                    from: 1
                                    to: 60
                                    value: root.settings.backupInterval
                                    onValueModified: {
                                        var s = root.settings;
                                        s.backupInterval = value;
                                        if (SettingsManager)
                                            SettingsManager.settings = s;

                                    }
                                }

                            }

                        }

                    }

                    GroupBox {
                        title: "編集"
                        Layout.fillWidth: true

                        ColumnLayout {
                            RowLayout {
                                Label {
                                    text: "最大Undo回数:"
                                }

                                SpinBox {
                                    from: 10
                                    to: 1000
                                    value: root.settings.undoCount
                                    onValueModified: {
                                        var s = root.settings;
                                        s.undoCount = value;
                                        if (SettingsManager)
                                            SettingsManager.settings = s;

                                    }
                                }

                            }

                            Label {
                                text: "※ 数値を大きくするとメモリ消費量が増加します"
                                font.pixelSize: 11
                                color: "gray"
                            }

                        }

                    }

                    GroupBox {
                        title: "エクスポート"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2

                            Label {
                                text: "連番画像品質 (PNG):"
                            }

                            SpinBox {
                                from: 0
                                to: 100
                                value: root.settings.exportImageQuality
                                onValueModified: {
                                    var s = root.settings;
                                    s.exportImageQuality = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                            Label {
                                text: "連番画像パディング桁数:"
                            }

                            SpinBox {
                                from: 2
                                to: 10
                                value: root.settings.exportSequencePadding
                                onValueModified: {
                                    var s = root.settings;
                                    s.exportSequencePadding = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                        }

                    }

                    GroupBox {
                        title: "起動"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2

                            Label {
                                text: "スプラッシュ表示時間 (ms):"
                            }

                            SpinBox {
                                from: 0
                                to: 5000
                                stepSize: 100
                                value: root.settings.splashDuration
                                onValueModified: {
                                    var s = root.settings;
                                    s.splashDuration = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                            Label {
                                text: "スプラッシュ画像サイズ (px):"
                            }

                            SpinBox {
                                from: 128
                                to: 2048
                                stepSize: 64
                                value: root.settings.splashSize
                                onValueModified: {
                                    var s = root.settings;
                                    s.splashSize = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                            Label {
                                text: "アプリ起動後遅延 (ms):"
                            }

                            SpinBox {
                                from: 100
                                to: 5000
                                stepSize: 100
                                value: root.settings.appStartupDelay
                                onValueModified: {
                                    var s = root.settings;
                                    s.appStartupDelay = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                        }

                    }

                }

            }

            // パフォーマンス設定
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    spacing: 15

                    GroupBox {
                        title: "メモリ・キャッシュ"
                        Layout.fillWidth: true

                        ColumnLayout {
                            RowLayout {
                                Label {
                                    text: "最大画像サイズ:"
                                }

                                ComboBox {
                                    model: ["1280x720", "1920x1080", "2560x1440", "3840x2160"]
                                    currentIndex: model.indexOf(root.settings.maxImageSize) !== -1 ? model.indexOf(root.settings.maxImageSize) : 1
                                    onActivated: {
                                        var s = root.settings;
                                        s.maxImageSize = currentText;
                                        if (SettingsManager)
                                            SettingsManager.settings = s;

                                    }
                                }

                            }

                            Label {
                                text: "※ AviUtlの「最大画像サイズ」に相当。VRAM使用量に影響します。"
                                font.pixelSize: 11
                                color: "gray"
                            }

                            RowLayout {
                                Label {
                                    text: "キャッシュサイズ (MB):"
                                }

                                SpinBox {
                                    from: 128
                                    to: 16384
                                    stepSize: 128
                                    value: root.settings.cacheSize
                                    editable: true
                                    onValueModified: {
                                        var s = root.settings;
                                        s.cacheSize = value;
                                        if (SettingsManager)
                                            SettingsManager.settings = s;

                                    }
                                }

                            }

                            Label {
                                text: "※ 動画クリップごとのキャッシュ容量 (推奨: 512MB〜2048MB)"
                                font.pixelSize: 11
                                color: "gray"
                            }

                        }

                    }

                    GroupBox {
                        title: "レンダリング"
                        Layout.fillWidth: true

                        ColumnLayout {
                            RowLayout {
                                Label {
                                    text: "レンダー スレッド数:"
                                }

                                SpinBox {
                                    from: 0
                                    to: 32
                                    value: root.settings.renderThreads
                                    textFromValue: function(v) {
                                        return v === 0 ? "自動" : v;
                                    }
                                    onValueModified: {
                                        var s = root.settings;
                                        s.renderThreads = value;
                                        if (SettingsManager)
                                            SettingsManager.settings = s;

                                    }
                                }

                            }

                        }

                    }

                    GroupBox {
                        title: "高度な設定"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2

                            Label {
                                text: "テキスト描画パディング係数:"
                            }

                            SpinBox {
                                from: 10
                                to: 80
                                stepSize: 5
                                value: Math.round((root.settings.textPaddingMultiplier || 4) * 10)
                                textFromValue: function(value, locale) {
                                    return (value / 10).toFixed(1);
                                }
                                valueFromText: function(text, locale) {
                                    return Math.round(parseFloat(text) * 10);
                                }
                                onValueModified: {
                                    var s = root.settings;
                                    s.textPaddingMultiplier = value / 10;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                        }

                    }

                    GroupBox {
                        title: "レイアウト (再起動後に適用)"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2

                            Label {
                                text: "トラックの高さ:"
                            }

                            SpinBox {
                                from: 20
                                to: 100
                                value: root.settings.timelineTrackHeight
                                onValueModified: {
                                    var s = root.settings;
                                    s.timelineTrackHeight = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                            Label {
                                text: "ヘッダーの高さ:"
                            }

                            SpinBox {
                                from: 20
                                to: 100
                                value: root.settings.timelineHeaderHeight
                                onValueModified: {
                                    var s = root.settings;
                                    s.timelineHeaderHeight = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                            Label {
                                text: "ルーラーの高さ:"
                            }

                            SpinBox {
                                from: 20
                                to: 100
                                value: root.settings.timelineRulerHeight
                                onValueModified: {
                                    var s = root.settings;
                                    s.timelineRulerHeight = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                            Label {
                                text: "レイヤーヘッダー幅:"
                            }

                            SpinBox {
                                from: 40
                                to: 300
                                value: root.settings.timelineLayerHeaderWidth
                                onValueModified: {
                                    var s = root.settings;
                                    s.timelineLayerHeaderWidth = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                            Label {
                                text: "ルーラー時間幅:"
                            }

                            SpinBox {
                                from: 40
                                to: 300
                                value: root.settings.timelineRulerTimeWidth
                                onValueModified: {
                                    var s = root.settings;
                                    s.timelineRulerTimeWidth = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                            Label {
                                text: "リサイズハンドル幅:"
                            }

                            SpinBox {
                                from: 5
                                to: 30
                                value: root.settings.timelineClipResizeHandleWidth
                                onValueModified: {
                                    var s = root.settings;
                                    s.timelineClipResizeHandleWidth = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                        }

                    }

                }

            }

            // タイムライン設定
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    spacing: 15

                    GroupBox {
                        title: "操作"
                        Layout.fillWidth: true

                        ColumnLayout {
                            RowLayout {
                                Label {
                                    text: "時間表示単位:"
                                }

                                ComboBox {
                                    model: ["フレーム (Frame)", "時間 (Time)"]
                                    currentIndex: root.settings.timeUnit === "time" ? 1 : 0
                                    onActivated: {
                                        var newSettings = root.settings;
                                        newSettings.timeUnit = index === 1 ? "time" : "frame";
                                        if (SettingsManager) {
                                            SettingsManager.settings = newSettings;
                                            SettingsManager.save();
                                        }
                                    }
                                }

                            }

                            CheckBox {
                                text: "オブジェクトをスナップする"
                                checked: root.settings.enableSnap
                                onToggled: {
                                    var s = root.settings;
                                    s.enableSnap = checked;
                                    if (SettingsManager) {
                                        SettingsManager.settings = s;
                                        SettingsManager.save();
                                    }
                                }
                            }

                            CheckBox {
                                text: "中間点追加・分割を常に現在フレームで行う"
                                checked: root.settings.splitAtCursor
                                onToggled: {
                                    var s = root.settings;
                                    s.splitAtCursor = checked;
                                    if (SettingsManager) {
                                        SettingsManager.settings = s;
                                        SettingsManager.save();
                                    }
                                }
                            }

                            CheckBox {
                                text: "対象レイヤー範囲を表示する"
                                checked: root.settings.showLayerRange
                                onToggled: {
                                    var s = root.settings;
                                    s.showLayerRange = checked;
                                    if (SettingsManager) {
                                        SettingsManager.settings = s;
                                        SettingsManager.save();
                                    }
                                }
                            }

                            RowLayout {
                                Label {
                                    text: "最大レイヤー数:"
                                }

                                SpinBox {
                                    from: 16
                                    to: 256
                                    value: root.settings.timelineMaxLayers
                                    onValueModified: {
                                        var s = root.settings;
                                        s.timelineMaxLayers = value;
                                        if (SettingsManager)
                                            SettingsManager.settings = s;

                                    }
                                }

                            }

                        }

                    }

                }

            }

            // 外観設定
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    spacing: 15

                    GroupBox {
                        title: "外観"
                        Layout.fillWidth: true

                        RowLayout {
                            Label {
                                text: "テーマ:"
                            }

                            ComboBox {
                                model: ["Dark", "Light", "Classic"]
                                currentIndex: model.indexOf(root.settings.theme) !== -1 ? model.indexOf(root.settings.theme) : 0
                                onActivated: {
                                    var s = root.settings;
                                    s.theme = currentText;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                        }

                    }

                }

            }

            // デフォルトプロジェクト設定
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    spacing: 15

                    GroupBox {
                        title: "新規プロジェクトの既定値"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2

                            Label {
                                text: "幅:"
                            }

                            SpinBox {
                                from: 1
                                to: 8000
                                value: root.settings.defaultProjectWidth
                                onValueModified: {
                                    var s = root.settings;
                                    s.defaultProjectWidth = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                            Label {
                                text: "高さ:"
                            }

                            SpinBox {
                                from: 1
                                to: 8000
                                value: root.settings.defaultProjectHeight
                                onValueModified: {
                                    var s = root.settings;
                                    s.defaultProjectHeight = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                            Label {
                                text: "FPS:"
                            }

                            SpinBox {
                                from: 1
                                to: 240
                                value: root.settings.defaultProjectFps
                                onValueModified: {
                                    var s = root.settings;
                                    s.defaultProjectFps = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                            Label {
                                text: "デフォルトクリップ長(F):"
                            }

                            SpinBox {
                                from: 1
                                to: 1000
                                value: root.settings.defaultClipDuration
                                onValueModified: {
                                    var s = root.settings;
                                    s.defaultClipDuration = value;
                                    if (SettingsManager)
                                        SettingsManager.settings = s;

                                }
                            }

                        }

                    }

                }

            }

            // エクスポート設定
            ScrollView {
                contentWidth: availableWidth

                ColumnLayout {
                    spacing: 15

                    GroupBox {
                        title: "映像デフォルト"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2

                            Label {
                                text: "コーデック:"
                            }

                            ComboBox {
                                model: ["h264_vaapi", "h264_nvenc", "h264_amf", "libx264", "hevc_vaapi", "hevc_nvenc", "libx265", "libaom-av1"]
                                currentIndex: model.indexOf(root.settings.exportDefaultCodec ?? "h264_vaapi")
                                onActivated: set("exportDefaultCodec", currentText)
                            }

                            Label {
                                text: "ビットレート (Mbps):"
                            }

                            SpinBox {
                                from: 1
                                to: 500
                                value: root.settings.exportDefaultBitrateMbps ?? 15
                                onValueModified: set("exportDefaultBitrateMbps", value)
                            }

                            Label {
                                text: "CRF デフォルト値:"
                            }

                            SpinBox {
                                from: 0
                                to: 51
                                value: root.settings.exportDefaultCrf ?? 20
                                onValueModified: set("exportDefaultCrf", value)
                            }

                            Label {
                                text: "フレームグラブ タイムアウト (ms):"
                            }

                            SpinBox {
                                from: 500
                                to: 10000
                                stepSize: 100
                                value: root.settings.exportFrameGrabTimeoutMs ?? 2000
                                onValueModified: set("exportFrameGrabTimeoutMs", value)
                            }

                            Label {
                                text: "進捗更新間隔 (フレーム):"
                            }

                            SpinBox {
                                from: 1
                                to: 60
                                value: root.settings.exportProgressInterval ?? 5
                                onValueModified: set("exportProgressInterval", value)
                            }

                        }

                    }

                    GroupBox {
                        title: "音声デフォルト"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2

                            Label {
                                text: "コーデック:"
                            }

                            ComboBox {
                                model: ["aac", "libopus", "libmp3lame", "flac", "pcm_s16le"]
                                currentIndex: model.indexOf(root.settings.exportDefaultAudioCodec ?? "aac")
                                onActivated: set("exportDefaultAudioCodec", currentText)
                            }

                            Label {
                                text: "ビットレート (kbps):"
                            }

                            SpinBox {
                                from: 64
                                to: 320
                                stepSize: 32
                                value: root.settings.exportDefaultAudioBitrateKbps ?? 192
                                onValueModified: set("exportDefaultAudioBitrateKbps", value)
                            }

                        }

                    }

                }

            }

            // デコード / オーディオ設定
            ScrollView {
                contentWidth: availableWidth

                ColumnLayout {
                    spacing: 15

                    GroupBox {
                        title: "映像デコード"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2

                            Label {
                                text: "インデックス予約フレーム数:"
                            }

                            SpinBox {
                                from: 1800
                                to: 1e+06
                                stepSize: 1800
                                value: root.settings.videoDecoderIndexReserve ?? 108000
                                onValueModified: set("videoDecoderIndexReserve", value)
                            }

                            Label {
                                text: "最小キャッシュ (MB):"
                            }

                            SpinBox {
                                from: 32
                                to: 4096
                                stepSize: 32
                                value: root.settings.videoDecoderMinCacheMB ?? 64
                                onValueModified: set("videoDecoderMinCacheMB", value)
                            }

                            Label {
                                text: "HW フレームプールサイズ:"
                            }

                            SpinBox {
                                from: 8
                                to: 128
                                stepSize: 8
                                value: root.settings.hwFramePoolSize ?? 32
                                onValueModified: set("hwFramePoolSize", value)
                            }

                        }

                    }

                    GroupBox {
                        title: "オーディオ"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2

                            Label {
                                text: "チャンネル数:"
                            }

                            ComboBox {
                                model: ["1 (モノラル)", "2 (ステレオ)"]
                                currentIndex: (root.settings.audioChannels ?? 2) - 1
                                onActivated: set("audioChannels", currentIndex + 1)
                            }

                            Label {
                                text: "プラグイン最大ブロックサイズ:"
                            }

                            ComboBox {
                                model: [128, 256, 512, 1024, 2048, 4096]
                                currentIndex: model.indexOf(root.settings.audioPluginMaxBlockSize ?? 4096)
                                onActivated: set("audioPluginMaxBlockSize", model[currentIndex])
                            }

                        }

                    }

                    GroupBox {
                        title: "タイムライン編集"
                        Layout.fillWidth: true

                        GridLayout {
                            columns: 2

                            Label {
                                text: "クリップ最小フレーム数:"
                            }

                            SpinBox {
                                from: 1
                                to: 30
                                value: root.settings.minClipDurationFrames ?? 5
                                onValueModified: set("minClipDurationFrames", value)
                            }

                            Label {
                                text: "マグネットスナップ範囲 (px):"
                            }

                            SpinBox {
                                from: 0
                                to: 50
                                value: root.settings.magneticSnapRange ?? 10
                                onValueModified: set("magneticSnapRange", value)
                            }

                            Label {
                                text: "最近使ったプロジェクト 最大表示数:"
                            }

                            SpinBox {
                                from: 3
                                to: 30
                                value: root.settings.recentProjectMaxCount ?? 10
                                onValueModified: set("recentProjectMaxCount", value)
                            }

                            Label {
                                text: "Lua フック間隔 (ms):"
                            }

                            SpinBox {
                                from: 8
                                to: 100
                                stepSize: 4
                                value: root.settings.luaHookIntervalMs ?? 16
                                onValueModified: set("luaHookIntervalMs", value)
                            }

                            Label {
                                text: "※ Lua フック間隔は再起動後に有効"
                                font.pixelSize: 10
                                color: palette.mid
                                Layout.columnSpan: 2
                            }

                        }

                    }

                }

            }

        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 10

            Button {
                text: "OK"
                highlighted: true
                onClicked: {
                    if (SettingsManager) {
                        SettingsManager.settings = root.settings;
                        SettingsManager.save();
                    }
                    root.hide();
                }
            }

            Button {
                text: "キャンセル"
                onClicked: root.hide()
            }

            Button {
                text: "適用"
                onClicked: {
                    if (SettingsManager) {
                        SettingsManager.settings = root.settings;
                        SettingsManager.save();
                    }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 15

                    Repeater {
                        model: ["LADSPA", "DSSI", "LV2", "VST2", "VST3", "CLAP", "SF2", "SFZ", "JSFX"]

                        delegate: GroupBox {
                            title: modelData
                            Layout.fillWidth: true

                            ColumnLayout {
                                width: parent.width

                                CheckBox {
                                    text: qsTr("Enable %1").arg(modelData)
                                    checked: root.settings["pluginEnable" + modelData] ?? true
                                    onToggled: {
                                        var s = root.settings;
                                        s["pluginEnable" + modelData] = checked;
                                        if (SettingsManager)
                                            SettingsManager.settings = s;

                                    }
                                }

                                Label {
                                    text: qsTr("Custom Search Paths (one path per line):")
                                    color: "gray"
                                    font.pixelSize: 11
                                }

                                ScrollView {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 80
                                    clip: true

                                    TextArea {
                                        text: (root.settings["pluginPaths" + modelData] || []).join("
")
                                        wrapMode: TextArea.NoWrap
                                        onEditingFinished: {
                                            var s = root.settings;
                                            var pathsArray = text.split("
").map((p) => {
                                                return p.trim();
                                            }).filter((p) => {
                                                return p.length > 0;
                                            });
                                            s["pluginPaths" + modelData] = pathsArray;
                                            if (SettingsManager)
                                                SettingsManager.settings = s;

                                        }

                                        background: Rectangle {
                                            color: palette.base
                                            border.color: "#444"
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
