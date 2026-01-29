import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    MultiEffect {
        width: root.width
        height: root.height
        source: root.source
        brightness: (root.evalNumber("brightness", 100) - 100) / 100.0
        saturation: (root.evalNumber("saturation", 100) - 100) / 100.0
        contrast: (root.evalNumber("contrast", 100) - 100) / 100.0
    }
}