import Qt5Compat.GraphicalEffects
import QtQuick
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

    // 1. 共通ブラー (FastBlur)
    FastBlur {
        id: baseBlur

        anchors.fill: parent
        source: root.source
        radius: root.commonRadius
        transparentBorder: true
        visible: false
    }

    // 2. 横方向の追加ブラー (DirectionalBlur)
    DirectionalBlur {
        id: hBlur

        anchors.fill: parent
        source: baseBlur
        angle: 90 // 90度回転して適用されるため、0度方向(右)へのブラーとなる(Qt仕様)
        length: root.remH * 2 // FastBlurのradius換算に合わせるため2倍
        samples: Math.min(32, Math.max(8, length))
        transparentBorder: true
        visible: false
    }

    // 3. 縦方向の追加ブラー (DirectionalBlur)
    DirectionalBlur {
        id: vBlur

        anchors.fill: parent
        source: root.remH > 0.1 ? hBlur : baseBlur
        angle: 0 // 0度回転 = 下方向(Qt仕様)
        length: root.remV * 2
        samples: Math.min(32, Math.max(8, length))
        transparentBorder: true
        visible: root.source !== null && root.blurAlpha
    }

    // 4. 透明度の境界をぼかさない (マスキング)
    OpacityMask {
        anchors.fill: parent
        source: root.remV > 0.1 ? vBlur : (root.remH > 0.1 ? hBlur : baseBlur)
        maskSource: root.source // 元画像のアルファチャンネルで切り抜く
        visible: root.source !== null && !root.blurAlpha
    }

}
