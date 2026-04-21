import "../common" as Common
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root

    // 親から受け取るドラフト設定
    required property var draftSettings

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

    // ページ本体
    Layout.fillWidth: true
    Layout.fillHeight: true
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: root.availableWidth
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
