import QtQuick
import QtQuick3D
import QtQuick.Effects
import "../effects"

Node {
    id: root
    property real sizeW: 100
    property real sizeH: 100
    property color color: "#66aa99"
    property real opacity: 1.0

    // データモデル: C++のEffectModel(QObject)リストを受け取る
    property list<QtObject> effectModels: TimelineBridge ? TimelineBridge.getClipEffectsModel(modelData.id) : []

    // フィルタパラメータの抽出 (リアクティブ)
    property var blurModel: {
        for(let i=0; i<effectModels.length; i++) 
            if(effectModels[i].id === "blur") return effectModels[i];
        return null;
    }
    
    // バインディング: 値が変われば即座に反映
    property real blurRadius: (blurModel && blurModel.enabled) ? (blurModel.params.size || 0) : 0

    Item {
        id: sourceItem
        // 自動パディング: ぼかし半径に応じてキャンバスを拡張しクリッピングを防ぐ
        property int padding: root.blurRadius > 0 ? Math.ceil(root.blurRadius * 3) : 0
        width: root.sizeW + padding * 2
        height: root.sizeH + padding * 2
        visible: false 

        Rectangle {
            anchors.centerIn: parent
            width: root.sizeW
            height: root.sizeH
            color: root.color
        }
    }

    // 汎用エフェクトチェーン (MultiEffectを内包)
    EffectChain {
        id: effector
        sourceItem: sourceItem
        effectModels: root.effectModels
        anchors.fill: sourceItem
        visible: false
    }

    Model {
        source: "#Rectangle"
        // 3Dモデルへのマッピング
        scale: Qt.vector3d(effector.width / 100, effector.height / 100, 1)
        opacity: root.opacity
        materials: DefaultMaterial {
            diffuseMap: Texture { sourceItem: effector }
            lighting: DefaultMaterial.NoLighting
            blendMode: DefaultMaterial.SourceOver
        }
    }
}
