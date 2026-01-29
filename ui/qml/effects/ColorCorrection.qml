import QtQuick
import QtQuick.Effects

Item {
    id: root
    property var params: ({})
    property Item source

    MultiEffect {
        anchors.fill: parent
        source: root.source
        
        property real p_bright: (root.params && root.params["brightness"] !== undefined ? Number(root.params["brightness"]) : 100)
        property real p_sat: (root.params && root.params["saturation"] !== undefined ? Number(root.params["saturation"]) : 100)
        property real p_cont: (root.params && root.params["contrast"] !== undefined ? Number(root.params["contrast"]) : 100)

        brightness: (p_bright - 100) / 100.0
        saturation: (p_sat - 100) / 100.0
        contrast: (p_cont - 100) / 100.0
    }
}