import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

MenuItem {
    id: control
    property string iconName: ""

    contentItem: RowLayout {
        spacing: 6

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
            opacity: enabled ? 1.0 : 0.3
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
            opacity: enabled ? 1.0 : 0.3
            color: control.highlighted ? control.palette.highlightedText : control.palette.text
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            visible: text !== ""
            Layout.leftMargin: 12
        }
    }
}