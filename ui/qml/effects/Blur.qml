import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root
    MultiEffect {
        // anchors ではなく明示的サイズ（親 Item のサイズを継承）
        width: root.width
        height: root.height
        source: root.source
        blurEnabled: true
        blurMax: 64
        blur: root.evalNumber("size", 0) / 64.0
    }
}