import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real size: Math.max(0, root.evalNumber("size", 5))
    property real aspect: Math.max(-100, Math.min(100, root.evalNumber("aspect", 0)))
    property bool blurAlpha: root.evalParam("blur_alpha", true)
    // 縦横比の計算 (-100: 縦のみ, 0: 等倍, 100: 横のみ)
    readonly property real hRatio: aspect < 0 ? (100 + aspect) / 100 : 1
    readonly property real vRatio: aspect > 0 ? (100 - aspect) / 100 : 1
    readonly property real hLen: size * hRatio
    readonly property real vLen: size * vRatio
    // 最適化: 共通部分はFastBlurで処理し、差分のみDirectionalBlurで行う
    readonly property real commonRadius: Math.min(hLen, vLen)
    readonly property real remH: hLen - commonRadius
    readonly property real remV: vLen - commonRadius

    // MultiEffectの標準描画を無効化
    maskEnabled: true
    maskSource: emptyMask

    MultiEffect {
        anchors.fill: parent
        source: root.sourceProxy
        blurEnabled: true
        blurMax: Math.max(32, root.size)
        blur: root.size / blurMax
        maskEnabled: !root.blurAlpha
        maskSource: !root.blurAlpha ? root.sourceProxy : null
    }

}
