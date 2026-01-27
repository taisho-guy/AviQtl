import QtQuick
import QtQuick3D
import QtQuick.Effects
import ".."

Node {
    id: root
    property string textContent: "Text"
    property int textSize: 64
    property color color: "white"
    property real opacity: 1.0
    property int clipId: -1

    property list<QtObject> rawEffectModels: TimelineBridge && clipId >= 0 ? TimelineBridge.getClipEffectsModel(clipId) : []

    // フィルタリング
    property var filterModels: {
        var list = [];
        for(var i=0; i<rawEffectModels.length; i++) {
            var eff = rawEffectModels[i];
            if (eff.id !== "rect" && eff.id !== "text" && eff.id !== "transform") {
                list.push(eff);
            }
        }
        return list;
    }
    
    function getBlurPadding() {
        for(let i=0; i<rawEffectModels.length; i++) {
            if(rawEffectModels[i].id === "blur" && rawEffectModels[i].enabled) {
                return (rawEffectModels[i].params["size"] || 0);
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
        // Node内のItemは3Dシーンに直接描画されないため、テクスチャ更新用にTrueにする
        visible: true

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
        effectModels: root.filterModels
        width: sourceItem.width
        height: sourceItem.height
        visible: true
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
