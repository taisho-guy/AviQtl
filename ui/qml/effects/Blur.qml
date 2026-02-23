import Qt5Compat.GraphicalEffects
import QtQuick
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

    // 1. 光の強さ (前処理: コントラスト強調)
    // 輝度が高い部分をより強調するためにコントラストを上げる
    BrightnessContrast {
        id: gainEffect

        anchors.fill: parent
        source: root.source
        // gain 0 -> contrast 0 (通常)
        // gain 100 -> contrast 1.0 (最大)
        contrast: root.gain / 100
        brightness: root.gain / 200 // 少し明るくする
        visible: false
    }

    // 2. 共通ブラー (FastBlur)
    // 縦横等倍成分を高速に処理
    FastBlur {
        id: baseBlur

        anchors.fill: parent
        source: gainEffect
        radius: root.commonRadius
        transparentBorder: true
        visible: false
    }

    // 3. 横方向の追加ブラー (DirectionalBlur)
    DirectionalBlur {
        id: hBlur

        anchors.fill: parent
        source: root.remH > 0.1 ? baseBlur : baseBlur // 変更なし（ロジック整理のため）
        angle: 90 // 90度回転して適用されるため、0度方向(右)へのブラーとなる(Qt仕様)
        length: root.remH * 2 // FastBlurのradius換算に合わせるため2倍
        samples: Math.min(32, Math.max(8, length))
        transparentBorder: true
        visible: false
    }

    // 4. 縦方向の追加ブラー (DirectionalBlur)
    DirectionalBlur {
        id: vBlur

        anchors.fill: parent
        // 最適化: 横方向の追加ブラーが不要な場合はスキップして直結する
        source: root.remH > 0.1 ? hBlur : baseBlur
        angle: 0 // 0度回転 = 下方向(Qt仕様)
        length: root.remV * 2
        samples: Math.min(32, Math.max(8, length))
        transparentBorder: true
        visible: root.source !== null && !root.fixedSize
    }

    // 5. サイズ固定 (マスキング)
    OpacityMask {
        anchors.fill: parent
        // 最適化: 最終出力のソースを選択
        source: root.remV > 0.1 ? vBlur : (root.remH > 0.1 ? hBlur : baseBlur)
        maskSource: root.source // 元画像のアルファチャンネルで切り抜く
        visible: root.source !== null && root.fixedSize
    }

}
