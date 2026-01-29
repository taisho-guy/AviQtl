import QtQuick
import QtQuick3D
import "." as Common

Node {
    id: base
    
    // CompositeView から自動注入されるプロパティ
    property int clipId: -1
    property int clipStartFrame: 0
    property int clipDurationFrames: 0
    
    // 自動計算プロパティ
    readonly property int currentFrame: TimelineBridge ? TimelineBridge.currentFrame : 0
    readonly property int relFrame: currentFrame - clipStartFrame
    readonly property list<QtObject> rawEffectModels: (TimelineBridge && clipId > 0) 
        ? TimelineBridge.getClipEffectsModel(clipId) : []
    
    // フィルタ系エフェクト（transform/object以外）
    readonly property var filterModels: {
        var list = [];
        for(var i=0; i<rawEffectModels.length; i++) {
            var eff = rawEffectModels[i];
            if (eff.id !== "rect" && eff.id !== "text" && eff.id !== "transform") {
                list.push(eff);
            }
        }
        return list;
    }
    
    // 【統一API】キーフレーム優先評価（全オブジェクトで使用可能）
    function evalParam(effectId, paramName, fallback) {
        for(let i=0; i<rawEffectModels.length; i++) {
            let eff = rawEffectModels[i];
            if (eff.id === effectId && eff.enabled) {
                if (eff.evaluatedParam) {
                    var v = eff.evaluatedParam(paramName, relFrame);
                    if (v !== undefined && v !== null) return v;
                }
                return eff.params[paramName] !== undefined ? eff.params[paramName] : fallback;
            }
        }
        return fallback;
    }
    
    // ぼかしパディング自動計算（全オブジェクト共通）
    function getBlurPadding() {
        for(let i=0; i<rawEffectModels.length; i++) {
            if(rawEffectModels[i].id === "blur" && rawEffectModels[i].enabled) {
                var v = rawEffectModels[i].evaluatedParam 
                    ? rawEffectModels[i].evaluatedParam("size", relFrame) 
                    : undefined;
                if (v === undefined || v === null) v = rawEffectModels[i].params["size"];
                return Number(v || 0);
            }
        }
        return 0;
    }
    readonly property real padding: getBlurPadding()
    
    // 子クラスがオーバーライドするプロパティ
    property Item sourceItem: Item {
        // デフォルトはダミー（visible: falseは子側で設定）
    }
    
    // sourceItem は常に非表示（renderer.output のみ表示）
    onSourceItemChanged: if (sourceItem) sourceItem.visible = false
    property alias renderer: rendererInstance
    
    // レンダラー自動配置
    Common.ObjectRenderer {
        id: rendererInstance
        originalSource: base.sourceItem
        effectModels: base.filterModels
        relFrame: base.relFrame
    }
}