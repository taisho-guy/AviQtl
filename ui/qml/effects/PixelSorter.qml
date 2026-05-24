import QtQuick
import QtQuick.Controls
import "qrc:/qt/qml/AviQtl/ui/qml/common" as Common

Common.BaseComputeEffect {
    // デフォルト値など

    id: root

    // 1. 確実に fbCaptureItem を持つ親を遡って探す
    source: {
        var p = parent;
        while (p) {
            // fbCaptureItem を持ち、かつそれがエフェクト自身ではなく、テクスチャソースとしての性質を持つか確認
            if (p.fbCaptureItem !== undefined && p.fbCaptureItem !== null && p.fbCaptureItem !== root && p.fbCaptureItem.hasOwnProperty("recursive"))
                return p.fbCaptureItem;

            p = p.parent;
        }
        // 初期化中の一時的な null は許容する
        return null;
    }
    // デバッグ用: バインディングの状態をコンソールに出力
    onSourceChanged: console.log("[PixelSorter] source updated to:", source)
    // Qt.resolvedUrl を使うことで、この QML ファイルと同じディレクトリにある QSB を絶対パスで指定できます
    computeShader: Qt.resolvedUrl("pixelsorter.comp.qsb")
    // C++ の params プロパティに直接マップする（BaseComputeEffect の実装に依存）
    params: ({
        "mix": 1
    })

    // デバッグ用: シェーダーエラーの表示
    Label {
        anchors.centerIn: parent
        text: "Compute Error:\n" + root.computeError
        color: "red"
        font.bold: true
        visible: root.computeError !== undefined && root.computeError !== ""
        horizontalAlignment: Text.AlignHCenter

        background: Rectangle {
            color: "black"
            opacity: 0.7
            radius: 4
        }

    }

}
