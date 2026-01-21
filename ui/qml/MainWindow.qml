import QtQuick 2.15
import QtQuick.Window 2.15
import "common" as Common

Common.RinaWindow {
    visible: true
    width: 640
    height: 360
    x: 100
    y: 100
    title: "Rina Main"
    
    // 重要: View3D の背後に黒背景を強制
    color: "#000000"
    
    CompositeView {
        anchors.fill: parent
    }
}