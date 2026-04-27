import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "common" as Common

Common.AviQtlWindow {
    id: root

    title: qsTr("パッケージマネージャー")
    width: 600
    height: 400
    minimumWidth: 500
    minimumHeight: 300

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true

            Button {
                text: qsTr("リポジトリを同期")
                icon.name: "refresh-line"
                enabled: PackageManager && !PackageManager.isBusy
                onClicked: PackageManager.refreshRepositories()
            }

            Item {
                Layout.fillWidth: true
            }

            TextField {
                placeholderText: qsTr("検索...")
                Layout.preferredWidth: 200
            }

        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: palette.base
            border.color: palette.mid
            border.width: 1

            Label {
                anchors.centerIn: parent
                text: PackageManager && PackageManager.isBusy ? PackageManager.statusText : qsTr("（後で実装: パッケージリスト）")
                color: palette.text
            }

        }

        ProgressBar {
            Layout.fillWidth: true
            visible: PackageManager && PackageManager.isBusy
            value: PackageManager ? PackageManager.progress : 0
        }

    }

}
