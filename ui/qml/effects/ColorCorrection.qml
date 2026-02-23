import Qt5Compat.GraphicalEffects 1.0
import QtQuick 2.15
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // Hue, Saturation, Lightness
    HueSaturation {
        id: hs

        anchors.fill: parent
        source: root.source
        hue: root.evalNumber("hue", 0) / 180
        saturation: (root.evalNumber("saturation", 100) - 100) / 100
        lightness: (root.evalNumber("lightness", 100) - 100) / 100
        visible: root.source !== null && root.width > 0 && root.height > 0
    }

    // Brightness, Contrast
    BrightnessContrast {
        anchors.fill: parent
        source: hs
        brightness: (root.evalNumber("brightness", 100) - 100) / 100
        contrast: (root.evalNumber("contrast", 100) - 100) / 100
        visible: hs.visible
    }

}
