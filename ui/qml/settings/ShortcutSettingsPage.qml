import "../common" as Common
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root

    // 親から受け取るドラフト設定
    required property var draftSettings
    required property var shortcutList

    // ショートカット変更シグナル
    signal shortcutValueChanged(string actionId, string value)

    function getShortcutValue(actionId, fallback) {
        if (!draftSettings["shortcuts"])
            return fallback;

        return draftSettings["shortcuts"][actionId] !== undefined ? draftSettings["shortcuts"][actionId] : fallback;
    }

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
                model: shortcutList

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
