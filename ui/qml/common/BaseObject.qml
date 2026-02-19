import "." as Common
import QtQuick
import QtQuick3D

Node {
    id: base

    // CompositeView 側から渡される「Window配下のItem」。ここに2D系を寄せる
    property Item renderHost: null
    property var owned2D: []
    // CompositeView から自動注入されるプロパティ
    property int clipId: -1
    property int clipStartFrame: 0
    property int clipDurationFrames: 0
    property var clipParams: ({
    })
    // CompositeView の FB 収集ロジックから参照される公開プロパティ
    // clipNode (CompositeView の delegate) から layer 番号を注入する
    // clipNode.clipLayerRole は model.layer と同期、
    // fbRendererOutput は NodeLoader.onItemChanged で接続される
    property int clipLayerRole: -1
    // CompositeView の clipNode から直接セット
    // FB 収集対象: 変換済み2Dキャプチャアイテム
    // FB 収集対象: 変換済み2Dキャプチャアイテム (外部から item.fbCaptureItem でアクセス可能)
    property alias fbCaptureItem: _fbCaptureItemImpl
    property Item fbRendererOutput: _fbCaptureItemImpl // onRenderHostChanged/onCompleted で fbCaptureItem に接続
    // CompositeView から注入される変換パラメータ
    property real clipNodeScaleX: 1
    property real clipNodeScaleY: 1
    property real clipNodePosX: 0
    property real clipNodePosY: 0
    property real clipNodeRotZ: 0
    property real clipNodeOpacity: 1
    // 自動計算プロパティ
    property int currentFrame: (TimelineBridge && TimelineBridge.transport) ? TimelineBridge.transport.currentFrame : 0
    readonly property int relFrame: currentFrame - clipStartFrame
    property var rawEffectModels: []
    // フィルタ系エフェクト（transform/object以外）
    readonly property var filterModels: {
        var res = [];
        for (var i = 0; i < rawEffectModels.length; i++) {
            var eff = rawEffectModels[i];
            if (eff.id !== "rect" && eff.id !== "text" && eff.id !== "image" && eff.id !== "video" && eff.id !== "audio" && eff.id !== "transform")
                res.push(eff);

        }
        return res;
    }
    readonly property real padding: getBlurPadding()
    // 子クラスがオーバーライドするプロパティ
    property Item sourceItem
    property alias renderer: rendererInstance

    function adopt2D(item) {
        if (!item || !renderHost)
            return ;

        if (item.parent === renderHost)
            return ;

        item.parent = renderHost;
        // visible を落とすと SceneGraph から外れてテクスチャ更新が止まり得るので触らない。
        // 表示は CompositeView 側の host opacity と ShaderEffectSource.hideSource に任せる。
        owned2D.push(item);
    }

    function refreshEffects() {
        rawEffectModels = (TimelineBridge && clipId > 0) ? TimelineBridge.getClipEffectsModel(clipId) : [];
    }

    // 【統一API】キーフレーム優先評価（全オブジェクトで使用可能）
    function evalParam(effectId, paramName, fallback) {
        // clipParamsはClipModelから渡されるフラットなパラメータマップ
        if (clipParams && clipParams[paramName] !== undefined)
            return clipParams[paramName];

        return fallback;
    }

    // ぼかしパディング自動計算（全オブジェクト共通）
    function getBlurPadding() {
        for (let i = 0; i < rawEffectModels.length; i++) {
            if (rawEffectModels[i].id === "blur" && rawEffectModels[i].enabled) {
                var v = rawEffectModels[i].evaluatedParam ? rawEffectModels[i].evaluatedParam("size", relFrame) : undefined;
                if (v === undefined || v === null)
                    v = rawEffectModels[i].params["size"];

                return Number(v || 0);
            }
        }
        return 0;
    }

    // renderHost が後からセットされても確実に移送する
    onRenderHostChanged: {
        adopt2D(sourceItem);
        adopt2D(rendererInstance);
        adopt2D(_fbCaptureItemImpl);
    }
    Component.onCompleted: {
        // 各オブジェクト(TextObject/RectObject)が set してくる sourceItem を移す
        adopt2D(base.sourceItem);
        // ObjectRenderer(= ShaderEffectSource/effectsチェーン)も移す
        adopt2D(rendererInstance);
        adopt2D(_fbCaptureItemImpl);
        refreshEffects();
    }
    Component.onDestruction: {
        for (var i = 0; i < owned2D.length; i++) {
            try {
                if (owned2D[i])
                    owned2D[i].destroy();

            } catch (e) {
            }
        }
        owned2D = [];
    }
    onClipIdChanged: refreshEffects()
    // sourceItem は常に非表示（renderer.output のみ表示）
    onSourceItemChanged: {
        if (sourceItem) {
            // キャプチャ安定化のためvisibleは落とさず、不可視化はopacityで行う
            sourceItem.visible = true;
            sourceItem.opacity = 0;
        }
    }

    Connections {
        function onClipEffectsChanged(changedClipId) {
            if (changedClipId === clipId)
                refreshEffects();

        }

        function onClipsChanged() {
            refreshEffects();
        }

        target: TimelineBridge
    }

    // ─── 2D変換済みキャプチャアイテム ─────────────────────────────
    // View3D の clipNode が持つ transform を 2D 空間で再現し、
    // FB が「変換後の最終見た目」を収集できるようにする
    Item {
        id: _fbCaptureItemImpl

        width: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.width : 1920
        height: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.height : 1080
        visible: true // SceneGraph に残すため true (opacity は renderHost 側で 0)

        Item {
            id: fbTransformItem

            // テクスチャサイズをスケール適用後のサイズに設定
            width: (renderer.output.sourceItem ? renderer.output.sourceItem.width : 1) * base.clipNodeScaleX
            height: (renderer.output.sourceItem ? renderer.output.sourceItem.height : 1) * base.clipNodeScaleY
            // AviUtl 座標系: 中心(0,0)、Y下プラス → Qt2D: 中心 = parent の center + offset
            x: _fbCaptureItemImpl.width / 2 + base.clipNodePosX - width / 2
            y: _fbCaptureItemImpl.height / 2 - base.clipNodePosY - height / 2
            rotation: -base.clipNodeRotZ
            opacity: base.clipNodeOpacity

            ShaderEffectSource {
                anchors.fill: parent
                sourceItem: renderer.finalItem
                live: true
                hideSource: true // finalItem (opacity:0 かもしれないが) を隠す
            }

        }

    }

    // レンダラー自動配置
    Common.ObjectRenderer {
        id: rendererInstance

        originalSource: base.sourceItem
        effectModels: base.filterModels
        relFrame: base.relFrame
    }

    sourceItem: Item {
        // デフォルトはダミー（visible: falseは子側で設定）
        width: 1
        height: 1
    }

}
