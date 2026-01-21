import QtQuick 2.15
import QtQuick.Window 2.15

Window {
    id: root
    
    // システムパレットを取得
    SystemPalette { 
        id: systemPalette
        colorGroup: SystemPalette.Active 
    }
    
    // 背景色をシステムに合わせる
    color: systemPalette.window
    
    // 子要素で使えるようにエイリアスを提供
    property alias palette: systemPalette
}