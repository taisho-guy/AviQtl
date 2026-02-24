import QtQuick
import QtQuick.Effects
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real size: Math.max(0, root.evalNumber("size", 10))
    property real aspect: Math.max(-100, Math.min(100, root.evalNumber("aspect", 0)))
    property real gain: Math.max(0, root.evalNumber("gain", 0))
    property bool fixedSize: root.evalParam("fixedSize", false)
    // 縦横比の計算 (-100: 縦のみ, 0: 等倍, 100: 横のみ)
    // aspect < 0: 横を減らす (1.0 -> 0.0)
    // aspect > 0: 縦を減らす (1.0 -> 0.0)
    readonly property real hRatio: aspect < 0 ? (100 + aspect) / 100 : 1
    readonly property real vRatio: aspect > 0 ? (100 - aspect) / 100 : 1
    readonly property real hLen: size * hRatio
    readonly property real vLen: size * vRatio
    // 最適化: 共通部分はFastBlurで処理し、差分のみDirectionalBlurで行う
    readonly property real commonRadius: Math.min(hLen, vLen)
    readonly property real remH: hLen - commonRadius
    readonly property real remV: vLen - commonRadius

    // MultiEffectの描画を無効化
    maskEnabled: true
    maskSource: emptyMask

    // 1. 横方向ブラー
    ShaderEffect {
        id: hPass

        property variant source: root.sourceProxy
        property vector2d direction: Qt.vector2d(1, 0)
        property real radius: root.hLen
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "blur.frag.qsb"
    }

    ShaderEffectSource {
        id: hPassProxy

        sourceItem: hPass
        hideSource: true
        visible: true
        opacity: 0
    }

    // 2. 縦方向ブラー + 出力
    ShaderEffect {
        id: vPass

        property variant source: hPassProxy
        property vector2d direction: Qt.vector2d(0, 1)
        property real radius: root.vLen
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "blur.frag.qsb"
    }

    ShaderEffectSource {
        id: vPassProxy

        sourceItem: vPass
        hideSource: root.fixedSize
        visible: true
        opacity: 0
    }

    // サイズ固定用マスク (必要な場合のみ適用)
    MultiEffect {
        anchors.fill: parent
        source: vPassProxy
        maskEnabled: root.fixedSize
        maskSource: root.sourceProxy
        visible: root.fixedSize
    }

}
