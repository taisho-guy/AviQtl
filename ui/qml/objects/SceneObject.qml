import QtQuick
import QtQuick.Shapes
import QtQuick3D
import Rina
import Rina.Core
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    // 将来的にはここで「子シーンをオフスクリーン描画」したテクスチャを貼る
    // MVPとしては境界可視化のみ

    id: root

    // TimelineBridge から現在のフレームやシーン情報を取得する前提
    property int targetSceneId: evalParam("scene", "targetSceneId", 0)
    property real speed: evalParam("scene", "speed", 1)
    property int offset: evalParam("scene", "offset", 0)
    property real opacity: evalParam("scene", "opacity", 1)
    // シーン内時間計算
    property int sceneFrame: {
        var f = Math.floor(relFrame * speed) + offset;
        // シーン長が定義されていればクランプ
        var dur = TimelineBridge ? TimelineBridge.getSceneDuration(targetSceneId) : 0;
        if (dur > 0)
            f = Math.max(0, Math.min(f, dur - 1));

        return f;
    }
    // 更新検知用カウンタ
    property int updateCounter: 0

    // シーンデコーダー (C++)
    SceneDecoder {
        id: decoder

        targetSceneId: root.targetSceneId
        currentFrame: root.sceneFrame
        store: videoFrameStore // main.cppから注入されたコンテキストプロパティ
        timelineBridge: TimelineBridge
    }

    Connections {
        function onFrameUpdated(key) {
            if (key === decoder.frameKey)
                root.updateCounter++;

        }

        target: videoFrameStore
    }

    // 3Dモデルとして表示
    Model {
        source: "#Rectangle"
        scale: Qt.vector3d(root.sourceItem.width / 100, root.sourceItem.height / 100, 1)
        opacity: root.opacity

        materials: DefaultMaterial {
            lighting: DefaultMaterial.NoLighting
            blendMode: root.blendMode
            cullMode: root.cullMode

            diffuseMap: Texture {
                sourceItem: renderer.output
            }

        }

    }

    sourceItem: Item {
        property var sceneInfo: TimelineBridge ? TimelineBridge.getSceneInfo(root.targetSceneId) : null

        width: sceneInfo ? (sceneInfo.width || 1920) : ((TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.width : 1920)
        height: sceneInfo ? (sceneInfo.height || 1080) : ((TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.height : 1080)
        visible: false

        Image {
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            cache: false
            // デコーダーが生成したキーで画像を取得
            source: decoder.frameKey ? "image://videoFrame/" + decoder.frameKey + "?u=" + root.updateCounter : ""
        }

    }

}
