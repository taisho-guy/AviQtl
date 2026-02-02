import "." as Common
import QtQuick 2.15
import QtQuick3D 6.0

View3D {
    id: root

    // Inputs
    property var clipList: []
    // Can be QAbstractListModel or QVariantList
    property int currentFrame: 0
    property int projWidth: 1920
    property int projHeight: 1080
    property Item renderHost: null // For 2D items inside this scene
    // Component cache
    property var _componentCache: ({
    })

    function getCachedComponent(url) {
        if (_componentCache[url])
            return _componentCache[url];

        var c = Qt.createComponent(url, Component.Asynchronous);
        _componentCache[url] = c;
        return c;
    }

    PerspectiveCamera {
        id: mainCamera

        property real distance: root.projHeight / (2 * Math.tan(fieldOfView * Math.PI / 360))

        fieldOfView: 30
        position: Qt.vector3d(0, 0, distance)
        clipFar: 5000
    }

    DirectionalLight {
        eulerRotation.x: -30
        z: 1000
    }

    Node {
        id: sceneRoot
    }

    Instantiator {
        model: root.clipList
        onObjectAdded: (index, object) => {
            object.parent = sceneRoot;
        }
        onObjectRemoved: (index, object) => {
            object.parent = null;
        }

        delegate: Node {
            id: clipNode

            // Handle both QAbstractListModel (model) and QVariantList (modelData)
            property var clipData: (typeof modelData !== "undefined") ? modelData : model
            property int startFrame: clipData.startFrame
            property int duration: clipData.durationFrames
            property int relFrame: root.currentFrame - startFrame
            property bool isActive: relFrame >= 0 && relFrame < duration
            // Calculate Params dynamically
            property var p: ({
            })
            readonly property real px: p.x || 0
            readonly property real py: p.y || 0
            readonly property real pz: p.z || 0
            readonly property real pRotX: p.rotationX || 0
            readonly property real pRotY: p.rotationY || 0
            readonly property real pRotZ: p.rotationZ || 0
            readonly property real pScale: p.scale || 100
            readonly property real pAspect: p.aspect || 0
            readonly property real pOpacity: p.opacity || 1

            function updateParams() {
                if (!isActive)
                    return ;

                var effs = clipData.effectModels;
                if (!effs) {
                    // Fallback for static params (if any)
                    if (clipData.params)
                        p = clipData.params;

                    return ;
                }
                var combined = {
                };
                // Default values
                combined.x = 0;
                combined.y = 0;
                combined.z = 0;
                combined.scale = 100;
                combined.opacity = 1;
                for (var i = 0; i < effs.length; i++) {
                    var eff = effs[i];
                    if (eff && eff.enabled) {
                        var val = eff.evaluatedParams(relFrame);
                        for (var k in val) combined[k] = val[k]
                    }
                }
                p = combined;
            }

            visible: isActive
            onRelFrameChanged: updateParams()
            onClipDataChanged: updateParams()
            Component.onCompleted: updateParams()
            x: px
            y: -py
            z: pz + (clipData.layer * 5)
            pivot: Qt.vector3d(p.anchorX || 0, -(p.anchorY || 0), p.anchorZ || 0)
            eulerRotation.x: pRotX
            eulerRotation.y: -pRotY
            eulerRotation.z: -pRotZ
            scale: Qt.vector3d(pScale * 0.01 * (pAspect >= 0 ? 1 + pAspect : 1), pScale * 0.01 * (pAspect < 0 ? 1 - pAspect : 1), pScale * 0.01)
            opacity: pOpacity

            Common.NodeLoader {
                source: clipData.qmlSource || ""
                properties: {
                    "opacity": clipNode.pOpacity,
                    "clipId": clipData.id,
                    "clipStartFrame": clipData.startFrame,
                    "clipDurationFrames": clipData.durationFrames,
                    "renderHost": root.renderHost,
                    "clipParams": clipNode.p
                }
                componentFactory: root.getCachedComponent
            }

        }

    }

    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Transparent
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
    }

}
