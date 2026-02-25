import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

MenuItem {
    id: control

    property string iconName: ""

    // Binding loop対策: 暗黙の高さを明示的に計算し、レイアウトの循環依存を防止
    implicitHeight: Math.max(32, contentItem.implicitHeight + topPadding + bottomPadding)

    // デフォルトのインジケーター（チェックボックス）を無効化し、
    // contentItem内で自前で描画することでアイコンとの重なりを防ぐ
    indicator: Item {
    }

    contentItem: RowLayout {
        spacing: 6

        // チェックマーク (checkableな場合のみ表示・スペース確保)
        RinaIcon {
            visible: control.checkable
            iconName: "check_line"
            size: 18
            color: control.highlighted ? control.palette.highlightedText : control.palette.text
            opacity: control.checked ? 1 : 0
            Layout.alignment: Qt.AlignVCenter
        }

        // アイコン
        RinaIcon {
            iconName: control.iconName
            size: 18
            color: control.highlighted ? control.palette.highlightedText : control.palette.text
            visible: control.iconName !== ""
            Layout.alignment: Qt.AlignVCenter
            Layout.rightMargin: 4
        }

        // メニューテキスト
        Text {
            text: control.text
            font: control.font
            opacity: enabled ? 1 : 0.3
            color: control.highlighted ? control.palette.highlightedText : control.palette.text
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        // ショートカットキー表示
        Text {
            text: control.action ? control.action.shortcut : ""
            font: control.font
            opacity: enabled ? 1 : 0.3
            color: control.highlighted ? control.palette.highlightedText : control.palette.text
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            visible: text !== ""
            Layout.leftMargin: 12
        }

    }

}
