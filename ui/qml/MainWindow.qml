import QtQuick 2.15
import QtQuick.Window 2.15
import "common" as Common

Common.RinaWindow {
    visible: true
    width: 640
    height: 360
    x: 100; y: 100
    title: "Rina Main"
    
    CompositeView {
        anchors.fill: parent
    }
}