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
    // Workspace.currentTimeline.currentSceneId 等が使えない場合、単純にclipIdだけで良いが、
    // 動画表示時は必ずこのオブジェクトの親の currentFrame が変わるので
    property string instanceKey: String(base.clipId)

    // ★ ここが一番のキモ:
    // この VideoObject が属しているシーンの時間が進んだとき (relFrame は BaseObject の property)
    // C++ 側に「このクリップのこのローカル時間でデコードして！」と要求を出す
    onRelFrameChanged: {
        if (Workspace.currentTimeline && typeof Workspace.currentTimeline.requestVideoFrame === "function" && base.clipId > 0)
            Workspace.currentTimeline.requestVideoFrame(base.clipId, base.relFrame);

    }
    Component.onCompleted: {
        if (Workspace.currentTimeline && typeof Workspace.currentTimeline.requestVideoFrame === "function" && base.clipId > 0)
            Workspace.currentTimeline.requestVideoFrame(base.clipId, base.relFrame);

    }

    Connections {
        function onFrameUpdated(key) {
            if (key === base.instanceKey)
                base.updateCounter++;

        }

        target: videoFrameStore
    }

    sourceItem: Item {
        id: containerItem

        // Node 内で初期化される際、width は Qt Quick のレイアウトから外れて 0 になることがある。
        // implicitWidth/Height を明示的に設定し、ObjectRenderer 側でそれをフォールバックとして拾う。
        implicitWidth: (Workspace.currentTimeline && Workspace.currentTimeline.project) ? Workspace.currentTimeline.project.width : 1920
        implicitHeight: (Workspace.currentTimeline && Workspace.currentTimeline.project) ? Workspace.currentTimeline.project.height : 1080
        width: implicitWidth
        height: implicitHeight

        VideoOutput {
            id: videoOut

            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectFit
            opacity: base.opacity
            Component.onCompleted: {
                if (base.clipId > 0 && typeof videoFrameStore !== "undefined")
                    videoFrameStore.registerSink(base.instanceKey, videoOut.videoSink);

            }

            Connections {
                function onClipIdChanged() {
                    if (base.clipId > 0 && typeof videoFrameStore !== "undefined")
                        videoFrameStore.registerSink(base.instanceKey, videoOut.videoSink);

                }

                function onInstanceKeyChanged() {
                    if (base.clipId > 0 && typeof videoFrameStore !== "undefined")
                        videoFrameStore.registerSink(base.instanceKey, videoOut.videoSink);

                }

                target: base
            }

        }

    }

}
