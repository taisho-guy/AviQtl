import QtMultimedia
import QtQuick
import QtQuick3D
import "qrc:/qt/qml/AviQtl/ui/qml/common" as Common

Common.BaseObject {
    id: base

    property string imagePath: String(evalParam("image", "path", ""))
    property int fillMode: Number(evalParam("image", "fillMode", VideoOutput.PreserveAspectFit))
    property real imageOpacity: Number(evalParam("image", "opacity", 1))
    property int updateCounter: 0
    property string instanceKey: String(base.clipId)

    sourceItem: containerItem
    onImagePathChanged: {
        if (Workspace.currentTimeline && typeof Workspace.currentTimeline.requestImageLoad === "function" && base.clipId > 0)
            Workspace.currentTimeline.requestImageLoad(base.clipId, base.imagePath);

    }
    Component.onCompleted: {
        if (Workspace.currentTimeline && typeof Workspace.currentTimeline.requestImageLoad === "function" && base.clipId > 0)
            Workspace.currentTimeline.requestImageLoad(base.clipId, base.imagePath);

    }

    Connections {
        function onFrameUpdated(key) {
            if (key === base.instanceKey)
                base.updateCounter++;

        }

        target: videoFrameStore
    }

    Item {
        id: containerItem

        readonly property real pad: 0 // paddingは削除済み

        // Node 内で初期化される際、width は Qt Quick のレイアウトから外れて 0 になることがある。
        // implicitWidth/Height を明示的に設定し、ObjectRenderer 側でそれをフォールバックとして拾う。
        implicitWidth: (Workspace.currentTimeline && Workspace.currentTimeline.project ? Workspace.currentTimeline.project.width : 1920)
        implicitHeight: (Workspace.currentTimeline && Workspace.currentTimeline.project ? Workspace.currentTimeline.project.height : 1080)
        width: implicitWidth
        height: implicitHeight

        VideoOutput {
            id: videoOut

            anchors.centerIn: parent
            width: (Workspace.currentTimeline && Workspace.currentTimeline.project) ? Workspace.currentTimeline.project.width : 1920
            height: (Workspace.currentTimeline && Workspace.currentTimeline.project) ? Workspace.currentTimeline.project.height : 1080
            fillMode: base.fillMode
            opacity: base.imageOpacity
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
