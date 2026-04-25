import QtQuick
import QtQuick3D
import "qrc:/qt/qml/AviQtl/ui/qml/common" as Common

Common.BaseObject {
    id: root

    // CompositeView が検知するフラグ (GroupControlObject の isGroupControl と同じ仕組み)
    property bool isCameraControl: true
    // AviUtl 互換パラメータ
    property real camX: Number(evalParam("camera", "x", 0))
    property real camY: Number(evalParam("camera", "y", 0))
    property real camZ: Number(evalParam("camera", "z", 0))
    property real tarX: Number(evalParam("camera", "tx", 0))
    property real tarY: Number(evalParam("camera", "ty", 0))
    property real tarZ: Number(evalParam("camera", "tz", 0))
    property real roll: Number(evalParam("camera", "roll", 0))
    property real fov: Math.max(1, Math.min(170, Number(evalParam("camera", "fov", 30))))
    // CompositeView が参照するカメラノード
    property alias camera: cam
    // デフォルトカメラ距離 (mainCamera と同じ計算式)
    readonly property real _defaultDist: {
        var h = (Workspace.currentTimeline && Workspace.currentTimeline.project) ? Workspace.currentTimeline.project.height : 1080;
        return h / (2 * Math.tan(fov * Math.PI / 360));
    }

    // CompositeView への登録・解除 (GroupControlObject と同じパターン)
    Component.onCompleted: {
        if (renderHost && renderHost.parent && renderHost.parent.registerCameraControl)
            renderHost.parent.registerCameraControl(root);

    }
    Component.onDestruction: {
        if (renderHost && renderHost.parent && renderHost.parent.unregisterCameraControl)
            renderHost.parent.unregisterCameraControl(root);

    }

    PerspectiveCamera {
        id: cam

        // lookAt で注視点を設定
        // Qt3D 6.x では Node.lookAt() が使えないため eulerRotation で代替
        // target オフセットから方向ベクトルを計算
        readonly property vector3d _target: Qt.vector3d(root.tarX, -root.tarY, root.tarZ)
        readonly property vector3d _dir: position.minus(_target).normalized()

        fieldOfView: root.fov
        clipFar: 5000
        // AviUtl 座標系 (Y下プラス) → Qt3D (Y上プラス): Y を反転
        position: Qt.vector3d(root.camX, -root.camY, root._defaultDist + root.camZ)
        // pitchとyawを direction から逆算
        eulerRotation: {
            var d = _dir;
            var pitch = Math.asin(-d.y) * 180 / Math.PI;
            var yaw = Math.atan2(d.x, d.z) * 180 / Math.PI;
            return Qt.vector3d(pitch, yaw, root.roll);
        }
    }

    sourceItem: Item {
        width: 1
        height: 1
        visible: false
    }

}
