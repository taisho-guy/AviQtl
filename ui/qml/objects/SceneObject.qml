import QtQuick
import QtQuick.Shapes
import QtQuick3D
import "qrc:/qt/qml/AviQtl/ui/qml" as Ui
import "qrc:/qt/qml/AviQtl/ui/qml/common" as Common

Common.BaseObject {
    // テクスチャソースとして用いるため可視化しない // removed: managed by BaseObject

    id: root

    property int targetSceneId: evalParam("scene", "targetSceneId", 0)
    property real speed: evalParam("scene", "speed", 1)
    property int offset: evalParam("scene", "offset", 0)
    property real opacity: evalParam("scene", "opacity", 1)
    // シーン内時間計算
    property int sceneFrame: {
        var f = Math.floor(relFrame * speed) + offset;
        // シーン長が定義されていればクランプ
        var dur = typeof Workspace.currentTimeline !== "undefined" ? Workspace.currentTimeline.getSceneDuration(targetSceneId) : 0;
        if (dur > 0)
            f = Math.max(0, Math.min(f, dur - 1));

        return f;
    }

    // 3Dモデルとして表示
    // 以前のImage/SceneDecoderベースの代わりに、SceneRendererを直接組み込む
    // GPU空間内でシーングラフとして完結させる
    sourceItem: Ui.SceneRenderer {
        sceneId: root.targetSceneId
        currentFrame: root.sceneFrame
        // timelineBridge: typeof Workspace.currentTimeline !== "undefined" ? Workspace.currentTimeline : null
        visible: false
    }

}
