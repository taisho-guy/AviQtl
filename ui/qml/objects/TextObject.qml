import QtQuick
import QtQuick3D
import QtQuick.Effects
import Rina
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Node {
    id: root
    
    readonly property bool dbgEnabled: Qt.application.arguments.indexOf("--rina-debug") !== -1
    function dbg(msg){ if (!dbgEnabled) return; console.log("[TextObject][clipId=" + clipId + "] " + msg); if (TimelineBridge && TimelineBridge.log) TimelineBridge.log("[TextObject][clipId=" + clipId + "] " + msg) }

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

    // CompositeView から注入される
    property int clipStartFrame: 0
    property int clipDurationFrames: 0
    property int relFrame: TimelineBridge ? (TimelineBridge.currentFrame - clipStartFrame) : 0

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

    // パディング計算（暫定）: エフェクトが増えてもここでcanvasを広げておく
    function getBlurPadding() {
        for(let i=0; i<rawEffectModels.length; i++) {
            if(rawEffectModels[i].id === "blur") {
                var v = rawEffectModels[i].evaluatedParam ? rawEffectModels[i].evaluatedParam("size", relFrame) : undefined;
                return Number(v !== undefined ? v : (rawEffectModels[i].params["size"] || 0));
            }
        }
        return 0;
    }
    property real padding: getBlurPadding()

    // --- テクスチャ生成パイプライン ---
    
    resources: [
        // 1. ソースコンテンツ (生のテキスト)
        // これが「オリジナルの映像」であり、エフェクトチェーンの始点となる
        Item {
            id: originalSource
            visible: true
            
            // エフェクトによる拡張はエフェクト側でやるべきだが、
            // 便宜上、最低限のキャンバスサイズは確保する
            width: textItem.implicitWidth + (root.padding * 4)
            height: textItem.implicitHeight + (root.padding * 4)
            
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

        // 2. 共通レンダラーへの委譲
        Common.ObjectRenderer {
            id: renderer
            originalSource: originalSource
            effectModels: root.filterModels
            relFrame: root.relFrame
        }
    ]

    Model {
        source: "#Rectangle"
        scale: Qt.vector3d(
            ((renderer.output.sourceItem ? renderer.output.sourceItem.width  : originalSource.width)  / 100),
            ((renderer.output.sourceItem ? renderer.output.sourceItem.height : originalSource.height) / 100),
            1
        )
        opacity: root.opacity
        materials: DefaultMaterial {
            diffuseMap: Texture { sourceItem: renderer.output }
            lighting: DefaultMaterial.NoLighting
            blendMode: DefaultMaterial.SourceOver
        }
    }
}
