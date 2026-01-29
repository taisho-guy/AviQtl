import QtQuick
import QtQuick3D
import QtQuick.Effects
import Rina

Node {
    id: root
    
    property int currentFrame: TimelineBridge ? TimelineBridge.currentFrame : 0

    property real sizeW: {
        var _ = currentFrame;
        var _2 = TimelineBridge ? TimelineBridge.selectedClipData : null;
        for(let i=0; i<rawEffectModels.length; i++) {
            let eff = rawEffectModels[i];
            if (eff.id === "rect" && eff.enabled) {
                return eff.evaluatedParam ? eff.evaluatedParam("sizeW", currentFrame) : eff.params["sizeW"];
            }
        }
        return 100;
    }

    property real sizeH: {
        var _ = currentFrame;
        var _2 = TimelineBridge ? TimelineBridge.selectedClipData : null;
        for(let i=0; i<rawEffectModels.length; i++) {
            let eff = rawEffectModels[i];
            if (eff.id === "rect" && eff.enabled) {
                return eff.evaluatedParam ? eff.evaluatedParam("sizeH", currentFrame) : eff.params["sizeH"];
            }
        }
        return 100;
    }

    property color color: {
        var _ = currentFrame;
        var _2 = TimelineBridge ? TimelineBridge.selectedClipData : null;
        for(let i=0; i<rawEffectModels.length; i++) {
            let eff = rawEffectModels[i];
            if (eff.id === "rect" && eff.enabled) {
                return eff.evaluatedParam ? eff.evaluatedParam("color", currentFrame) : eff.params["color"];
            }
        }
        return "#66aa99";
    }

    property real opacity: {
        var _ = currentFrame;
        var _2 = TimelineBridge ? TimelineBridge.selectedClipData : null;
        for(let i=0; i<rawEffectModels.length; i++) {
            let eff = rawEffectModels[i];
            if (eff.id === "rect" && eff.enabled) {
                return eff.evaluatedParam ? eff.evaluatedParam("opacity", currentFrame) : eff.params["opacity"];
            }
        }
        return 1.0;
    }

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
    property real blurRadius: (blurModel && blurModel.enabled) ? (blurModel.params["size"] || 0) : 0

    // --- テクスチャ生成パイプライン ---
    resources: [
        // sourceRoot階層を削除
        Item {
            id: sourceItem
            visible: true // hideSourceで隠れるのでtrueでOK
            // 自動パディング: ぼかし半径に応じてキャンバスを拡張しクリッピングを防ぐ
            property int padding: root.blurRadius > 0 ? Math.ceil(root.blurRadius * 3) : 0
            width: root.sizeW + padding * 2
            height: root.sizeH + padding * 2

            Rectangle {
                anchors.centerIn: parent
                width: root.sizeW
                height: root.sizeH
                color: root.color
            }
        },

        // 汎用エフェクトチェーン (MultiEffectを内包)
        MultiEffect {
            id: multiEffect
            source: sourceItem
            anchors.fill: sourceItem
            visible: true

            // --- パラメータバインディング ---
            
            // Blur
            property var blurModel: {
                for(var i=0; i<root.rawEffectModels.length; i++){
                    if(root.rawEffectModels[i].id === "blur" && root.rawEffectModels[i].enabled) return root.rawEffectModels[i];
                }
                return null;
            }
            blurEnabled: !!blurModel
            blurMax: 64 // 上限
            blur: blurModel ? (blurModel.params["size"] || 0) / 64.0 : 0.0

            // Color Correction
            property var colorModel: {
                for(var i=0; i<root.rawEffectModels.length; i++){
                    if(root.rawEffectModels[i].id === "color_correction" && root.rawEffectModels[i].enabled) return root.rawEffectModels[i];
                }
                return null;
            }
            brightness: colorModel ? ((colorModel.params["brightness"] || 100) - 100) / 100.0 : 0.0
            contrast:   colorModel ? ((colorModel.params["contrast"]   || 100) - 100) / 100.0 : 0.0
            saturation: colorModel ? ((colorModel.params["saturation"] || 100) - 100) / 100.0 : 0.0
        },

        // エフェクトパラメータ変更監視
        Repeater {
            model: root.rawEffectModels
            Item {
                Connections {
                    target: modelData
                    function onParamsChanged() { root.updateTexture() }
                }
            }
        },

        ShaderEffectSource {
            id: textureSource
            sourceItem: multiEffect
            hideSource: true
            live: false
            visible: true
            recursive: true
            FrameAnimation {
                running: true
                onTriggered: textureSource.scheduleUpdate()
            }
        }
    ]

    Model {
        source: "#Rectangle"
        // 3Dモデルへのマッピング
        scale: Qt.vector3d(sourceItem.width / 100, sourceItem.height / 100, 1)
        opacity: root.opacity
        materials: DefaultMaterial {
            diffuseMap: Texture { sourceItem: textureSource }
            lighting: DefaultMaterial.NoLighting
            blendMode: DefaultMaterial.SourceOver
        }
    }
}
