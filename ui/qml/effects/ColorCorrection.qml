import QtQuick 2.15
import Qt5Compat.GraphicalEffects 1.0
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // saturation 相当
    HueSaturation {
        id: hs
        anchors.fill: parent
        source: root.source
        saturation: (root.evalNumber("saturation", 100) - 100) / 100.0
        visible: root.source !== null && root.width > 0 && root.height > 0
    }

    // brightness/contrast 相当
    BrightnessContrast {
        anchors.fill: parent
        source: hs
        brightness: (root.evalNumber("brightness", 100) - 100) / 100.0
        contrast: (root.evalNumber("contrast", 100) - 100) / 100.0
        visible: hs.visible
    }
}