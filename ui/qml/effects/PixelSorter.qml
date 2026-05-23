import QtQuick
import "qrc:/qt/qml/AviQtl/ui/qml/common" as Common

Common.BaseComputeEffect {
    id: root

    // Compute Shader のファイルパスを指定するだけ
    computeShader: "pixelsorter.comp.qsb"
    uniformMapping: {
        "mix": "mixAmount"
    }
}
