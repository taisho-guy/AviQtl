import "../common" as Common
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root

    // 親から受け取るドラフト設定
    required property var draftSettings
    required property var pluginFormats

    // プラグイン設定変更シグナル
    signal valueChanged(string key, var value)
    signal pluginEnabledChanged(string formatName, bool enabled)
    signal pluginPathsChanged(string formatName, string textValue)

    function pluginPathsText(formatName) {
        var vals = draftSettings["pluginPaths" + formatName];
        return vals ? vals.join("\n") : "";
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

    // シグナルを emit するブリッジ関数
    // （CheckBox / TextArea から root.xxx() 形式で呼ばれるため実装が必要）
    function setPluginEnabled(formatName, enabled) {
        pluginEnabledChanged(formatName, enabled);
    }

    function setPluginPathsFromText(formatName, textValue) {
        pluginPathsChanged(formatName, textValue);
    }

    function carlaDiscoveryPathsText() {
        var vals = draftSettings["carlaDiscoverySearchPaths"];
        return vals ? vals.join("\n") : "";
    }

    function setCarlaDiscoveryPathsFromText(textValue) {
        var lines = textValue.split(/\r?\n/);
        var paths = [];
        for (var i = 0; i < lines.length; ++i) {
            var p = lines[i].trim();
            if (p.length > 0)
                paths.push(p);

        }
        valueChanged("carlaDiscoverySearchPaths", paths);
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
            text: qsTr("各形式ごとに有効化と検索パスを設定できます")
            color: palette.mid
            wrapMode: Text.WordWrap
        }

        // --- Carla プラグインホスト設定 ---
        GroupBox {
            title: qsTr("Carla (プラグインホスト)")
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                Label {
                    text: qsTr("リソースディレクトリ")
                    color: palette.mid
                }

                TextField {
                    Layout.fillWidth: true
                    text: root.valueOr("carlaResourceDir", "/usr/share/carla/resources")
                    onEditingFinished: root.valueChanged("carlaResourceDir", text)

                    background: Rectangle {
                        color: palette.base
                        border.color: palette.mid
                        radius: 4
                    }

                }

                Label {
                    text: qsTr("carla-discovery-native 検索パス")
                    color: palette.mid
                }

                TextArea {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 100
                    wrapMode: TextArea.NoWrap
                    selectByMouse: true
                    text: root.carlaDiscoveryPathsText()
                    onActiveFocusChanged: {
                        if (!activeFocus)
                            root.setCarlaDiscoveryPathsFromText(text);

                    }

                    background: Rectangle {
                        color: palette.base
                        border.color: palette.mid
                        radius: 4
                    }

                }

                Label {
                    text: qsTr("1行に1パスを入力します。先頭から順に検索し、存在するパスを使用します")
                    color: palette.mid
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

            }

        }

        Repeater {
            model: pluginFormats

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
