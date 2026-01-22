import QtQuick
import QtQuick3D
import QtQuick.Effects
import "../effects"

Node {
    id: root
    property string textContent: "Text"
    property int textSize: 64
    property color color: "white"
    property real opacity: 1.0
    property int clipId: -1

    property list<QtObject> effectModels: TimelineBridge ? TimelineBridge.getClipEffectsModel(clipId) : []
    
    function getBlurPadding() {
        for(let i=0; i<effectModels.length; i++) {
            if(effectModels[i].id === "blur" && effectModels[i].enabled) {
                return (effectModels[i].params["size"] || 0);
            }
        }
        return 0;
    }
    property real blurPadding: getBlurPadding()

    Item {
        id: sourceItem
        property int padding: root.blurPadding > 0 ? Math.ceil(root.blurPadding * 3) : 10
        width: textItem.implicitWidth + padding * 2
        height: textItem.implicitHeight + padding * 2
        visible: false

        Text {
            id: textItem
            anchors.centerIn: parent
            text: root.textContent
            font.pixelSize: root.textSize
            color: root.color
            style: Text.Outline; styleColor: "black"
            
            // 【最重要修正】 NativeRendering(CPU)を廃止しGPU描画へ変更
            renderType: Text.QtRendering 
            antialiasing: true
        }
    }

    EffectChain {
        id: effector
        sourceItem: sourceItem
        effectModels: root.effectModels
        width: sourceItem.width
        height: sourceItem.height
        visible: false
    }

    Model {
        source: "#Rectangle"
        scale: Qt.vector3d(effector.width / 100, effector.height / 100, 1)
        opacity: root.opacity
        materials: DefaultMaterial {
            diffuseMap: Texture { sourceItem: effector }
            lighting: DefaultMaterial.NoLighting
            blendMode: DefaultMaterial.SourceOver
        }
    }
}
