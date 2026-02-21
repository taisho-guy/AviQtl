import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "common" as Common

Common.RinaWindow {
    id: root

    // C++のSettingsManagerとバインド
    property var settings: SettingsManager ? SettingsManager.settings : ({
    })
    property alias currentTabIndex: bar.currentIndex

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

        }

    }

}
