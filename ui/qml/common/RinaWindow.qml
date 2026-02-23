import QtQuick
import QtQuick.Controls
import QtQuick.Window

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
