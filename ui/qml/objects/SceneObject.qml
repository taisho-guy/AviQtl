import "../" // For CompositeView access if needed
import "../common" as Common
import QtQuick 2.15
import QtQuick3D 6.0

Common.BaseObject {
    id: root

    property int targetSceneId: parseInt(evalParam("scene", "sceneId", "0"))
    property int startFrameOffset: parseInt(evalParam("scene", "startFrame", "0"))
    // シーン内での現在のフレーム
    property int internalFrame: root.relFrame + root.startFrameOffset

    sourceItem: sceneRenderer

    // オフスクリーンレンダリング用Item
    Item {
        id: sceneRenderer

        visible: false
        // シーンの解像度（本来はシーン設定から取得すべきだが、一旦プロジェクト設定を使用）
        width: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.width : 1920
        height: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.height : 1080

        // 背景
        Rectangle {
            anchors.fill: parent
            color: "black" // シーン背景色（透過設定があれば変更）
        }

        // 3Dビューポート（シーン内のオブジェクトを描画）
        View3D {
            anchors.fill: parent

            PerspectiveCamera {
                id: mainCamera

                z: 1000
            }

            Node {
                id: sceneRoot

                // シーン内のクリップを展開
                Repeater {
                    model: (TimelineBridge && root.targetSceneId >= 0) ? TimelineBridge.getSceneClips(root.targetSceneId) : []

                    delegate: Node {
                        // クリップのアクティブ判定
                        property var clipData: modelData
                        property int currentFrame: root.internalFrame
                        property bool isActive: currentFrame >= clipData.startFrame && currentFrame < (clipData.startFrame + clipData.durationFrames)

                        visible: isActive

                        // NodeLoaderを使って動的にオブジェクトをロード
                        Common.NodeLoader {
                            // 重要: BaseObjectのcurrentFrameを上書きして、親タイムラインではなく
                            // このシーン内での時間を渡す
                            // レンダリングホストは親のBaseObjectのものを使用（再帰的な2D描画のため）

                            source: clipData.qmlSource || ""
                            // プロパティ注入
                            properties: {
                                "clipId": clipData.id,
                                "clipStartFrame": clipData.startFrame,
                                "clipDurationFrames": clipData.durationFrames,
                                "clipParams": clipData.params || {
                                },
                                "currentFrame": root.internalFrame,
                                "renderHost": root.renderHost
                            }
                        }

                    }

                }

            }

            environment: SceneEnvironment {
                backgroundMode: SceneEnvironment.Color
                clearColor: "transparent"
                antialiasingMode: SceneEnvironment.MSAA
                antialiasingQuality: SceneEnvironment.High
            }

        }

    }

    // 3Dモデルとしてレンダリング結果を表示
    Model {
        source: "#Rectangle"
        scale: Qt.vector3d(sceneRenderer.width / 100, sceneRenderer.height / 100, 1)
        opacity: 1

        materials: DefaultMaterial {
            lighting: DefaultMaterial.NoLighting
            blendMode: DefaultMaterial.SourceOver

            diffuseMap: Texture {
                sourceItem: renderer.output
            }

        }

    }

}
