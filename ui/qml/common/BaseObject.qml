import QtQuick
import QtQuick3D
import "." as Common

Node {
    id: base

    // CompositeView 側から渡される「Window配下のItem」。ここに2D系を寄せる
    property Item renderHost: null
    property var owned2D: []

    function adopt2D(item) {
        if (!item || !renderHost) return
        if (item.parent === renderHost) return
        item.parent = renderHost
        // visible を落とすと SceneGraph から外れてテクスチャ更新が止まり得るので触らない。
        // 表示は CompositeView 側の host opacity と ShaderEffectSource.hideSource に任せる。
        owned2D.push(item)
    }

    // renderHost が後からセットされても確実に移送する
    onRenderHostChanged: {
        adopt2D(sourceItem)
        adopt2D(rendererInstance)
    }

    Component.onCompleted: {
        // 各オブジェクト(TextObject/RectObject)が set してくる sourceItem を移す
        adopt2D(base.sourceItem)
        // ObjectRenderer(= ShaderEffectSource/effectsチェーン)も移す
        adopt2D(rendererInstance)
    }

    Component.onDestruction: {
        for (var i = 0; i < owned2D.length; i++) {
            try {
                if (owned2D[i]) owned2D[i].destroy()
            } catch (e) {}
        }
        owned2D = []
    }

    // CompositeView から自動注入されるプロパティ
    property int clipId: -1
    property int clipStartFrame: 0
    property int clipDurationFrames: 0
    property var clipParams: ({})
    
    // 自動計算プロパティ
    readonly property int currentFrame: (TimelineBridge && TimelineBridge.transport) ? TimelineBridge.transport.currentFrame : 0
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
        // clipParamsはClipModelから渡されるフラットなパラメータマップ
        if (clipParams && clipParams[paramName] !== undefined) {
            return clipParams[paramName];
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
        width: 1
        height: 1
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