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
        // paddingRect is handled by the container (RectObject) padding logic.
        // Setting it here with negative offsets shifts the render origin causing clipping.
    }
}