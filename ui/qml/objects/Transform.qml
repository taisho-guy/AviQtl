import QtQuick
import QtQuick3D
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

// Transform エフェクト。
// BaseEffect を継承し、ecsCache から evalParam 経由でパラメーターを取得する。
// ecsCache は BaseObject が Qt.binding で注入するため、手動更新は不要。
Common.BaseEffect {
    id: root

    readonly property vector3d outputPosition: {
        const x = evalNumber("x", 0);
        const y = evalNumber("y", 0);
        const z = evalNumber("z", 0);
        // AviUtl座標系(Y下向き) → Qt3D座標系(Y上向き)
        return Qt.vector3d(x, -y, z);
    }
    readonly property vector3d outputRotation: {
        const rx = evalNumber("rotationX", 0);
        const ry = evalNumber("rotationY", 0);
        const rz = evalNumber("rotationZ", 0);
        return Qt.vector3d(rx, ry, -rz);
    }
    readonly property vector3d outputPivot: {
        const cx = evalNumber("cx", 0);
        const cy = evalNumber("cy", 0);
        const cz = evalNumber("cz", 0);
        return Qt.vector3d(cx, -cy, cz);
    }
    readonly property real outputOpacity: evalNumber("opacity", 1)
    readonly property int outputCullMode: evalParam("backfaceVisible", true) ? DefaultMaterial.NoCulling : DefaultMaterial.BackFaceCulling
    // 2D FrameBuffer キャプチャ用
    readonly property real output2dScale: Math.max(0, evalNumber("scale", 100)) / 100
    readonly property real output2dX: evalNumber("x", 0)
    readonly property real output2dY: evalNumber("y", 0)
    readonly property real output2dRotationZ: evalNumber("rotationZ", 0)
}
