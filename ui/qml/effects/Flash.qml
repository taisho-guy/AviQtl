import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 強さ 0-100、x/y 方向、合成方法 (上/下/光のみ)
    property real strength: Math.max(0, root.evalNumber("strength", 100))
    property real flashX: root.evalNumber("x", 0)
    property real flashY: root.evalNumber("y", 0)
    property real compositeMode: {
        var m = root.evalParam("compositeMode", "top");
        if (m === "bottom")
            return 1;

        if (m === "only")
            return 2;

        return 0;
    }

    ShaderEffect {
        id: flashPass

        property variant source: root.sourceProxy
        property real strength: root.strength
        property real flashX: root.flashX
        property real flashY: root.flashY
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        visible: false
        fragmentShader: "flash.frag.qsb"
    }

    ShaderEffectSource {
        id: flashProxy

        sourceItem: flashPass
        hideSource: false
        visible: false
        live: true
    }

    ShaderEffect {
        property variant source: root.sourceProxy
        property variant flashSource: flashProxy
        property real compositeMode: root.compositeMode

        anchors.fill: parent
        fragmentShader: "flash_composite.frag.qsb"
    }

}
