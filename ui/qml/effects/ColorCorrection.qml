import QtQuick 2.15
import Qt5Compat.GraphicalEffects

Item {
    property var params: ({})
    property variant source

    Colorize {
        anchors.fill: parent
        source: parent.source
        hue: 0
        saturation: (parent.params.saturation !== undefined ? parent.params.saturation : 100) / 100.0
        lightness: ((parent.params.brightness !== undefined ? parent.params.brightness : 100) - 100) / 100.0
    }
}