import QtQuick
import QtQuick3D
import QtQuick.Effects
import Rina

Node {
    id: root
    
    // 現在フレーム（依存関係の起点）
    property int currentFrame: TimelineBridge ? TimelineBridge.currentFrame : 0

    property string textContent: {
        // 依存関係を明示
        var _ = currentFrame; 
        // TimelineBridgeのシグナルへの依存を擬似的に作るため、プロパティアクセスを含める
        // selectedClipDataChangedシグナル自体はプロパティではないため直接バインドできないが、
        // selectedClipData プロパティを参照することで変更を検知できる可能性がある。
        var _2 = TimelineBridge ? TimelineBridge.selectedClipData : null;
        
        for(let i=0; i<rawEffectModels.length; i++) {
            let eff = rawEffectModels[i];
            if (eff.id === "text" && eff.enabled) {
                var val = eff.evaluatedParam ? eff.evaluatedParam("text", currentFrame) : undefined;
                if (val === undefined || val === null) {
                    val = eff.params["text"];
                }
                if (val !== undefined && val !== null) {
                    return String(val);
                }
            }
        }
        return "Text";
    }
    
    // CompositeView(model.params=evaluatedParams) から渡される前提に統一
    property int textSize: {
        var _ = currentFrame;
        var _2 = TimelineBridge ? TimelineBridge.selectedClipData : null;
        
        for(let i=0; i<rawEffectModels.length; i++) {
            let eff = rawEffectModels[i];
            if (eff.id === "text" && eff.enabled) {
                // text.jsonでは "textSize" だが、コードによっては "size" を使う場合もあるため両方試す
                var val = eff.evaluatedParam ? eff.evaluatedParam("textSize", currentFrame) : undefined;
                
                if (val === undefined || val === null) {
                    val = eff.params["textSize"];
                }
                
                // フォールバック: "size"
                if (val === undefined || val === null) {
                    val = eff.evaluatedParam ? eff.evaluatedParam("size", currentFrame) : undefined;
                }
                if (val === undefined || val === null) {
                    val = eff.params["size"];
                }

                if (val !== undefined && val !== null) {
                    return Number(val);
                }
            }
        }
        return 64;
    }

    property color color: {
        var _ = currentFrame;
        var _2 = TimelineBridge ? TimelineBridge.selectedClipData : null;
        
        for(let i=0; i<rawEffectModels.length; i++) {
            let eff = rawEffectModels[i];
            if (eff.id === "text" && eff.enabled) {
                var val = eff.evaluatedParam ? eff.evaluatedParam("color", currentFrame) : undefined;
                if (val === undefined || val === null) {
                    val = eff.params["color"];
                }
                if (val !== undefined && val !== null) {
                    return val;
                }
            }
        }
        return "#ffffff";
    }

    property real opacity: 1.0 // opacityはTransformで制御されることが多いが、Text独自にも持つ場合

    property int clipId: -1

    // 修正: TimelineBridge.clipsに依存させることで、クリップ変更時にリストを再取得
    property list<QtObject> rawEffectModels: {
        var _ = TimelineBridge ? TimelineBridge.clips : null // 依存関係トリガー
        return TimelineBridge && clipId >= 0 ? TimelineBridge.getClipEffectsModel(clipId) : []
    }
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

    // 更新関数
    function updateTexture() {
        textureSource.scheduleUpdate();
    }

    // --- テクスチャ生成パイプライン ---
    
    resources: [
        // 1. ソースコンテンツ (生のテキスト)
        Item {
            id: sourceItem
            visible: true
            
            // Textのレイアウト計算を待たずにサイズを確保するための工夫
            property int targetW: textItem.implicitWidth + padding * 2
            property int targetH: textItem.implicitHeight + padding * 2
            property int padding: root.blurPadding > 0 ? Math.ceil(root.blurPadding * 3) : 10
            
            // 重要: サイズが変わったら通知
            onTargetWChanged: root.updateTexture()
            onTargetHChanged: root.updateTexture()
            width: targetW
            height: targetH

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
        },

        EffectChain {
            id: effector
            sourceItem: sourceItem
            effectModels: root.filterModels
            width: sourceItem.width
            height: sourceItem.height
            visible: true 
        },

        // エフェクトパラメータ変更監視
        // 各EffectModelのparamsChangedシグナルをリッスンして、
        // テクスチャを更新する
        Repeater {
            model: root.rawEffectModels
            Item {
                Connections {
                    target: modelData
                    function onParamsChanged() { root.updateTexture() }
                }
            }
        },

        // 2. テクスチャ化プロキシ
        // effectorをレンダリングし、結果を保持するが画面には表示しない
        ShaderEffectSource {
            id: textureSource
            sourceItem: effector.outputItem
            hideSource: true // ソースを画面から隠す
            live: false      // 自動更新を切る (スレッドエラー回避)
            visible: true    // カリング回避のため visible: true (hideSourceがあるので画面には出ない)
            recursive: true  // 入れ子のエフェクトを許可
            
            // サイズ変更時にテクスチャバッファも追従させる
            textureSize: Qt.size(sourceItem.width, sourceItem.height)
        }
    ]

    FrameAnimation {
        running: true
        onTriggered: root.updateTexture()
    }

    Model {
        source: "#Rectangle"
        scale: Qt.vector3d(sourceItem.width / 100, sourceItem.height / 100, 1)
        opacity: root.opacity
        materials: DefaultMaterial {
            diffuseMap: Texture { sourceItem: textureSource }
            lighting: DefaultMaterial.NoLighting
            blendMode: DefaultMaterial.SourceOver
        }
    }
}
