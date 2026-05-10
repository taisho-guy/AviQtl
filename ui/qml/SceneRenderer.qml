import AviQtl.Rendering 1.0
import QtQuick

Item {
    id: root

    // Phase 2 以降で ECS / CoreBridge との同期に使用するプロパティ
    property int sceneId: -1
    property int currentFrame: 0

    FilamentCanvas {
        anchors.fill: parent
        sceneId: root.sceneId
        currentFrame: root.currentFrame
    }

}
