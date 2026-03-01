import QtQuick
import qrc:/qt/qml/Rinauiqml/common as Common

Common.BaseEffect {
    id: root

    // AviUtl: 色相範囲 0-360、彩度範囲 0-100、境界補正 0-100
    //         変換元色 (sourceColor)、変換先色 (targetColor)
    property color sourceColor: root.evalColor("sourceColor", "#ff0000")
    property color targetColor: root.evalColor("targetColor", "#0000ff")
    property real  hueRange:    Math.max(0, root.evalNumber("hueRange",  30))
    property real  satRange:    Math.max(0, Math.min(100, root.evalNumber("satRange",  100)))
    property real  boundary:    Math.max(0, Math.min(100, root.evalNumber("boundary",  10)))

    ShaderEffect {
        property variant source:      root.sourceProxy
        property color   sourceColor: root.sourceColor
        property color   targetColor: root.targetColor
        property real    hueRange:    root.hueRange
        property real    satRange:    root.satRange
        property real    boundary:    root.boundary
        anchors.fill: parent
        fragmentShader: "colorgamutconv.frag.qsb"
    }
}
