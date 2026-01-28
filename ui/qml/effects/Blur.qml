import QtQuick
import QtQuick.Effects

Item {
    id: root
    property var params: ({})
    property Item source 

    MultiEffect {
        anchors.fill: parent
        source: root.source
        
        blurEnabled: true
        blurMax: 100
        blur: (root.params && root.params.size !== undefined) ? (root.params.size / 100.0) : 0.0
        
        paddingRect: Qt.rect(-blurMax, -blurMax, width + blurMax*2, height + blurMax*2)
    }
}