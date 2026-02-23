import Qt5Compat.GraphicalEffects
import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
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

    // 1. ブラー用ソースの準備 (着色処理)
    Item {
        id: flashSource

        anchors.fill: parent
        visible: false
        layer.enabled: true

        // 元の色を使用する場合
        ShaderEffectSource {
            anchors.fill: parent
            sourceItem: root.source
            visible: root.useOriginalColor
        }

        // 指定色で塗りつぶす場合
        ColorOverlay {
            anchors.fill: parent
            source: root.source
            color: root.flashColor
            visible: !root.useOriginalColor
        }

    }

    // 2. 閃光の生成 (ZoomBlur)
    ZoomBlur {
        id: flashEffect

        anchors.fill: parent
        source: flashSource
        length: root.strength
        horizontalOffset: root.centerX
        verticalOffset: root.centerY
        samples: Math.min(128, Math.max(24, root.strength * 2)) // 強さに応じてサンプル数を調整
        transparentBorder: true
        visible: false
    }

    // 3. 合成 (サイズ固定なし)
    Item {
        id: composition

        anchors.fill: parent
        visible: !root.fixedSize

        // 後方に合成 (Flash -> Source)
        ShaderEffectSource {
            anchors.fill: parent
            sourceItem: flashEffect
            visible: root.mode === 1
        }

        // 元画像 (光成分のみ以外で表示)
        ShaderEffectSource {
            anchors.fill: parent
            sourceItem: root.source
            visible: root.mode === 1 // 後方合成時は上に重ねる
        }

        // 光成分のみ
        ShaderEffectSource {
            anchors.fill: parent
            sourceItem: flashEffect
            visible: root.mode === 2
        }

        // 前方に合成 (Source + Flash(Add))
        Blend {
            anchors.fill: parent
            visible: root.mode === 0
            source: root.source
            foregroundSource: flashEffect
            mode: "add"
        }

    }

    // 4. サイズ固定 (OpacityMaskで切り抜き)
    OpacityMask {
        anchors.fill: parent
        visible: root.fixedSize
        source: composition
        maskSource: root.source
    }

}
