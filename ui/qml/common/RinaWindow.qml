import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

ApplicationWindow {
    id: root

    // 子要素で使えるようにエイリアスを提供
    property alias palette: systemPalette

    // 背景色をシステムに合わせる
    color: systemPalette.window

    // システムパレットを取得
    SystemPalette {
        id: systemPalette

        colorGroup: SystemPalette.Active
    }

}
