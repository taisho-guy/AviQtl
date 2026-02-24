import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    // Simplified Flash using MultiEffect (ZoomBlur removed)

    id: root

    property real strength: Math.max(0, root.evalNumber("strength", 10))
    property real centerX: root.evalNumber("x", 0)
    property real centerY: root.evalNumber("y", 0)
    // 0: Forward, 1: Backward, 2: Light Only
    property int mode: {
        var m = root.evalParam("mode", "前方に合成");
        if (m === "前方に合成")
            return 0;

        if (m === "後方に合成")
            return 1;

        if (m === "光成分のみ")
            return 2;

        return Number(m) || 0;
    }
    property bool useOriginalColor: root.evalParam("useOriginalColor", true)
    property color flashColor: root.evalColor("color", "#ffffff")
    property bool fixedSize: root.evalParam("fixedSize", false)

    // MultiEffectの標準描画を無効化
    maskEnabled: true
    maskSource: emptyMask

    MultiEffect {
        anchors.fill: parent
        source: root.sourceProxy
        brightness: root.strength / 100
        colorization: root.useOriginalColor ? 0 : 1
        colorizationColor: root.flashColor
        maskEnabled: root.fixedSize
        maskSource: root.fixedSize ? root.sourceProxy : null
    }

}
