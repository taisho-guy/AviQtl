import QtMultimedia
import QtQuick
import QtQuick3D
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: base

    property string imagePath: String(evalParam("image", "path", ""))
    property int fillMode: Number(evalParam("image", "fillMode", VideoOutput.PreserveAspectFit))
    property real imageOpacity: Number(evalParam("image", "opacity", 1))
    property int updateCounter: 0
    property string instanceKey: String(base.clipId)

    sourceItem: containerItem
    onImagePathChanged: {
        if (TimelineBridge && typeof TimelineBridge.requestImageLoad === "function" && base.clipId > 0)
            TimelineBridge.requestImageLoad(base.clipId, base.imagePath);

    }
    Component.onCompleted: {
        if (TimelineBridge && typeof TimelineBridge.requestImageLoad === "function" && base.clipId > 0)
            TimelineBridge.requestImageLoad(base.clipId, base.imagePath);

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
        scale: Qt.vector3d((renderer.output.sourceItem ? renderer.output.sourceItem.width : base.sourceItem.width) / 100, (renderer.output.sourceItem ? renderer.output.sourceItem.height : base.sourceItem.height) / 100, 1)
        opacity: base.imageOpacity

        materials: DefaultMaterial {
            lighting: DefaultMaterial.NoLighting
            blendMode: base.blendMode
            cullMode: base.cullMode

            diffuseMap: Texture {
                sourceItem: renderer.output
            }

        }

    }

    Item {
        id: containerItem

        readonly property real pad: base.padding * 2

        width: (TimelineBridge && TimelineBridge.project ? TimelineBridge.project.width : 1920) + pad
        height: (TimelineBridge && TimelineBridge.project ? TimelineBridge.project.height : 1080) + pad
        visible: false

        VideoOutput {
            id: videoOut

            anchors.centerIn: parent
            width: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.width : 1920
            height: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.height : 1080
            fillMode: base.fillMode
            opacity: base.imageOpacity
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

                function onInstanceKeyChanged() {
                    if (base.clipId > 0 && typeof videoFrameStore !== "undefined")
                        videoFrameStore.registerSink(base.instanceKey, videoOut.videoSink);

                }

                target: base
            }

        }

    }

}
