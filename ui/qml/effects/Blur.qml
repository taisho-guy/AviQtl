import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real size: Math.max(0, root.evalNumber("size", 10))
    property real aspect: Math.max(-100, Math.min(100, root.evalNumber("aspect", 0)))
    property real gain: Math.max(0, root.evalNumber("gain", 0))
    property bool fixedSize: root.evalParam("fixedSize", false)
    readonly property real hRatio: aspect < 0 ? (100 + aspect) / 100 : 1
    readonly property real vRatio: aspect > 0 ? (100 - aspect) / 100 : 1
    readonly property real hLen: size * hRatio
    readonly property real vLen: size * vRatio

    // 1. 横方向ブラー (常に非表示, hPassProxy がテクスチャとして利用)
    ShaderEffect {
        id: hPass

        property variant source: root.sourceProxy
        property vector2d direction: Qt.vector2d(1, 0)
        property real radius: root.hLen
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "blur.frag.qsb"
        visible: false
    }

    // hPass の結果を中間テクスチャとして保持
    ShaderEffectSource {
        id: hPassProxy

        sourceItem: hPass
        hideSource: false
        visible: false
        live: true
    }

    // 2. 縦方向ブラー
    // fixedSize=false の場合はこれが最終出力として直接 root に描画される
    // fixedSize=true の場合は非表示にし、mask が直接 source として参照する
    ShaderEffect {
        id: vPass

        property variant source: hPassProxy
        property vector2d direction: Qt.vector2d(0, 1)
        property real radius: root.vLen
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "blur.frag.qsb"
        visible: !root.fixedSize
    }

    // 3. サイズ固定用マスク
    // vPassProxy を介さず vPass を直接 source に渡す
    ShaderEffect {
        id: maskPass

        property variant source: vPass
        property variant maskSource: root.sourceProxy

        anchors.fill: parent
        visible: root.fixedSize
        fragmentShader: "mask.frag.qsb"
    }

}
