import QtMultimedia
import QtQuick
import QtQuick3D
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: base

    property string path: String(evalParam("video", "path", ""))
    property string playMode: String(evalParam("video", "playMode", "開始フレーム＋再生速度"))
    property int startFrame: Number(evalParam("video", "startFrame", 0))
    property real speed: Number(evalParam("video", "speed", 100))
    property int directFrame: Math.ceil(Number(evalParam("video", "directFrame", 0)))
    property real opacity: Number(evalParam("video", "opacity", 1))
    property var source: undefined
    property var params: ({
    })
    property var effectModel: null
    property int frame: 0
    property int width: containerItem.width
    property int height: containerItem.height
    property int updateCounter: 0
    // インスタンスごとにユニークなキーを生成（シーンID + クリップID）
    // TimelineBridge.currentSceneId 等が使えない場合、単純にclipIdだけで良いが、
    // 動画表示時は必ずこのオブジェクトの親の currentFrame が変わるので
    property string instanceKey: String(base.clipId)

    // ★ ここが一番のキモ:
    // この VideoObject が属しているシーンの時間が進んだとき (relFrame は BaseObject の property)
    // C++ 側に「このクリップのこのローカル時間でデコードして！」と要求を出す
    onRelFrameChanged: {
        if (TimelineBridge && typeof TimelineBridge.requestVideoFrame === "function" && base.clipId > 0)
            TimelineBridge.requestVideoFrame(base.clipId, base.relFrame);

    }
    Component.onCompleted: {
        if (TimelineBridge && typeof TimelineBridge.requestVideoFrame === "function" && base.clipId > 0)
            TimelineBridge.requestVideoFrame(base.clipId, base.relFrame);

    }

    Connections {
        function onFrameUpdated(key) {
            if (key === base.instanceKey)
                base.updateCounter++;

        }

        target: videoFrameStore
    }

    Model {
        source: "#Rectangle"
        scale: Qt.vector3d(base.sourceItem.width / 100, base.sourceItem.height / 100, 1)
        opacity: base.opacity

        materials: DefaultMaterial {
            lighting: DefaultMaterial.NoLighting
            blendMode: DefaultMaterial.SourceOver

            diffuseMap: Texture {
                sourceItem: renderer.output
            }

        }

    }

    sourceItem: Item {
        id: containerItem

        width: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.width : 1920
        height: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.height : 1080
        visible: false

        VideoOutput {
            id: videoOut

            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectFit
            opacity: base.opacity
            // FBOキャプチャの黒画面制約を突破するGPUレイヤー化
            layer.enabled: true
            layer.format: ShaderEffectSource.RGBA
            Component.onCompleted: {
                if (base.clipId > 0 && typeof videoFrameStore !== "undefined")
                    videoFrameStore.registerSink(base.instanceKey, videoOut.videoSink);

            }

            Connections {
                function onClipIdChanged() {
                    if (base.clipId > 0 && typeof videoFrameStore !== "undefined")
                        videoFrameStore.registerSink(base.instanceKey, videoOut.videoSink);

                }

                target: base
            }

        }

    }

}
