import QtQuick
import QtQuick.Controls
import "qrc:/qt/qml/AviQtl/ui/qml/common" as Common

Common.BaseComputeEffect {
    // デフォルト値など

    id: root

    // 直接の親アイテムから fbCaptureItem を取得。
    // エフェクトが Loader 内にある場合は parent.parent 等が必要になる可能性があるため、動的解決を利用
    source: (typeof fbCaptureItem !== "undefined") ? fbCaptureItem : (parent ? parent.fbCaptureItem : null)
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
