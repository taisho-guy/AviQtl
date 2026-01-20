import QtQuick 2.15
import QtQuick.Window 2.15

Window {
    visible: true
    width: 640
    height: 360
    x: 100; y: 100
    title: "Rina Main"
    color: "#000000"
    
    CompositeView {
        anchors.fill: parent
    }
}