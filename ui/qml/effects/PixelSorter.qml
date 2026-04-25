import QtQuick
import "qrc:/qt/qml/AviQtl/ui/qml/common" as Common

Common.BaseComputeEffect {
    id: root

    // Compute Shader のファイルパスを指定するだけ
    computeShader: "pixelsorter.comp.qsb"
    // JSON側の "mix" とシェーダー側の "mixAmount" (予約語回避) をマッピング
    uniformMapping: {
        "mix": "mixAmount"
    }
}
