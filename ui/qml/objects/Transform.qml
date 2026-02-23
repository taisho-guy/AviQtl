import QtQuick
import QtQuick3D
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

// このエフェクトはロジックを持たず、BaseObject.qml で直接処理されるため、中身は空で構いません。
// BaseEffectを継承してevalParamなどを使えるようにする
Common.BaseEffect {
    id: root

    // 計算結果を保持するプロパティ (3D Node用)
    readonly property vector3d outputPosition: {
        // evalNumberはBaseEffectから継承
        const x = evalNumber("x", 0);
        const y = evalNumber("y", 0);
        const z = evalNumber("z", 0);
        // AviUtl座標系(Y下向き)からQt3D座標系(Y上向き)へ変換
        return Qt.vector3d(x, -y, z);
    }
    readonly property vector3d outputRotation: {
        const rx = evalNumber("rotationX", 0);
        const ry = evalNumber("rotationY", 0);
        const rz = evalNumber("rotationZ", 0);
        // Z軸の回転方向が逆
        return Qt.vector3d(rx, ry, -rz);
    }
    readonly property real outputOpacity: evalNumber("opacity", 1)
    // 計算結果を保持するプロパティ (2D FrameBufferキャプチャ用)
    readonly property real output2dScale: Math.max(0, evalNumber("scale", 100)) / 100
    readonly property real output2dX: evalNumber("x", 0)
    readonly property real output2dY: evalNumber("y", 0)
    readonly property real output2dRotationZ: evalNumber("rotationZ", 0)
}
