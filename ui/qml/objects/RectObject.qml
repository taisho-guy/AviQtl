import QtQuick
import QtQuick3D
import QtQuick.Effects
import ".."

Node {
    id: root
    property real sizeW: 100
    property real sizeH: 100
    property color color: "#66aa99"
    property real opacity: 1.0
    // 追加: 親からIDを受け取る
    property int clipId: -1

    // 生のデータモデルを取得
    property list<QtObject> rawEffectModels: TimelineBridge && clipId >= 0 ? TimelineBridge.getClipEffectsModel(clipId) : []

    // エフェクトチェーン用に、オブジェクト定義(rect)やTransformを除外したリストを生成
    property var filterModels: {
        var list = [];
        for(var i=0; i<rawEffectModels.length; i++) {
            var eff = rawEffectModels[i];
            // "rect", "text", "transform" は描画フィルタではないため除外
            if (eff.id !== "rect" && eff.id !== "text" && eff.id !== "transform") {
                list.push(eff);
            }
        }
        return list;
    }

    // フィルタパラメータの抽出 (リアクティブ)
    property var blurModel: {
        for(let i=0; i<rawEffectModels.length; i++) 
            if(rawEffectModels[i].id === "blur") return rawEffectModels[i];
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
        visible: true

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
        effectModels: root.filterModels
        anchors.fill: sourceItem
        visible: true
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
