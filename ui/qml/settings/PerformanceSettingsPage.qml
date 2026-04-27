import "../common" as Common
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root

    // 親から受け取るドラフト設定
    required property var draftSettings
    required property var renderThreadValues
    required property var renderThreadLabels

    // 設定変更シグナル
    signal valueChanged(string key, var value)

    function valueOr(key, fb) {
        return draftSettings[key] !== undefined ? draftSettings[key] : fb;
    }

    function indexOfValue(values, target, fallback) {
        for (var i = 0; i < values.length; ++i) if (values[i] === target) {
            return i;
        }
        return fallback;
    }

    // --- ページ本体 ---
    Layout.fillWidth: true
    Layout.fillHeight: true
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: root.availableWidth
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
                    model: renderThreadLabels
                    currentIndex: root.indexOfValue(renderThreadValues, root.valueOr("renderThreads", 0), 0)
                    onActivated: root.setValue("renderThreads", renderThreadValues[currentIndex])
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
