import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "common" as Common

Common.RinaWindow {
    id: root

    // C++のSettingsManagerとバインド (初期値としてコピーを取得)
    property var settings: SettingsManager ? SettingsManager.settings : ({
    })

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
                            }

                            CheckBox {
                                text: "自動バックアップを有効化"
                                checked: root.settings.enableAutoBackup
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
                                }

                            }

                            Label {
                                text: "※ 数値を大きくするとメモリ消費量が増加します"
                                font.pixelSize: 11
                                color: "gray"
                            }

                        }

                    }

                }

            }

            // --- パフォーマンス設定 ---
            ScrollView {
                contentWidth: availableWidth

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
                                    currentIndex: 1
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
                                    from: 512
                                    to: 65536
                                    stepSize: 512
                                    value: root.settings.cacheSize
                                    editable: true
                                }

                            }

                            Label {
                                text: "※ 推奨: 実装メモリの1/4〜1/2程度 (例: 32GBなら8192MB)"
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
                                }

                            }

                        }

                    }

                }

            }

            // --- タイムライン設定 ---
            ScrollView {
                contentWidth: availableWidth

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

                        }

                    }

                }

            }

            // --- 外観設定 ---
            ScrollView {
                contentWidth: availableWidth

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
                                model: ["ダーク", "ライト", "クラシック"]
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
                    if (SettingsManager)
                        SettingsManager.save();

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
