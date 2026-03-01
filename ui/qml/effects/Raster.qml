import QtQuick
import qrc:/qt/qml/Rinauiqml/common as Common

Common.BaseEffect {
    id: root

    // AviUtl: 横幅/高さ/周期/縦ラスター/ランダム振幅
    property real  amplitude:   Math.max(0, root.evalNumber("amplitude",  50))
    property real  frequency:   Math.max(0, root.evalNumber("frequency",  50))
    property real  period:      root.evalNumber("period",   1)
    property real  time:        root.evalNumber("time",     0)
    property bool  vertical:    root.evalParam("vertical",  false)
    property real  randomAmp:   Math.max(0, Math.min(1, root.evalNumber("randomAmplitude", 0) / 100.0))

    ShaderEffect {
        property variant source:       root.sourceProxy
        property real    amplitude:    root.amplitude
        property real    frequency:    root.frequency
        property real    period:       root.period
        property real    time:         root.time
        property real    vertical:     root.vertical ? 1.0 : 0.0
        property real    randomAmp:    root.randomAmp
        property real    targetWidth:  root.width
        property real    targetHeight: root.height
        anchors.fill: parent
        fragmentShader: "raster.frag.qsb"
    }
}
