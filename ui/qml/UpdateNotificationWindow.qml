import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "common" as Common

Common.RinaWindow {
    id: root

    // UpdateChecker コンテキストプロパティから値を受け取る
    property string latestVersion: UpdateChecker ? UpdateChecker.latestVersion : ""
    property string releaseUrl: UpdateChecker ? UpdateChecker.releaseUrl : ""
    property string releaseNotes: UpdateChecker ? UpdateChecker.releaseNotes : ""

    title: qsTr("新しいバージョンがあります")
    width: 440
    height: 260
    minimumWidth: 440
    maximumWidth: 440
    minimumHeight: 260
    maximumHeight: 260

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // タイトル行（アイコン + バージョン文字列）
        RowLayout {
            spacing: 12
            Layout.fillWidth: true

            Label {
                text: "\uE98C" // ri-notification-3-line (Remix Icon)
                font.family: "remixicon"
                font.pixelSize: 28
                color: palette.highlight
            }

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true

                Label {
                    text: qsTr("Rina %1 がリリースされました").arg(root.latestVersion)
                    font.pixelSize: 15
                    font.bold: true
                    color: palette.text
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Label {
                    text: qsTr("新しいバージョンが Codeberg で公開されています。")
                    font.pixelSize: 12
                    color: palette.mid
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

            }

        }

        // リリースノート（最大4行）
        Label {
            visible: root.releaseNotes.length > 0
            text: root.releaseNotes
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            maximumLineCount: 4
            elide: Text.ElideRight
            color: palette.text
            opacity: 0.85
            leftPadding: 8
            topPadding: 4
            bottomPadding: 4

            background: Rectangle {
                color: palette.alternateBase
                radius: 4
            }

        }

        Item {
            Layout.fillHeight: true
        }

        // ボタン行
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("後で")
                onClicked: {
                    root.hide();
                }
            }

            Button {
                text: qsTr("リリースページを開く")
                highlighted: true
                onClicked: {
                    Qt.openUrlExternally(root.releaseUrl);
                    UpdateChecker.acknowledge();
                    root.hide();
                }
            }

            Button {
                text: qsTr("確認した")
                onClicked: {
                    UpdateChecker.acknowledge();
                    root.hide();
                }
            }

        }

    }

}
