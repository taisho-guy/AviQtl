import "." as Common
import QtQuick
import QtQuick3D

Node {
    // object エフェクト（text/rect/image 等）のパラメータ変更検知
    // _tmRev と同じカウンタ方式: property var 配列要素への直接依存は
    // QML エンジンが追跡できないため Connections 経由で強制通知する
    // 【統一API】キーフレーム優先評価（全オブジェクトで使用可能）
    // _paramRev を読むことで Connections→onParamsChanged() への依存を確立する。
    // property var の配列要素に対する直接依存は QML エンジンが追跡できないため、
    // _tmRev と同じカウンタ方式を採用する。
    // getBlurPadding は ObjectRenderer.totalExpansion に置換済みのため削除
    // ─── 2D変換済みキャプチャアイテム ─────────────────────────────
    // View3D の clipNode が持つ transform を 2D 空間で再現し、
    // FB が「変換後の最終見た目」を収集できるようにする

    id: base

    // CompositeView 側から渡される「Window配下のItem」。ここに2D系を寄せる
    property Item renderHost: null
    property var owned2D: []
    // CompositeView から自動注入されるプロパティ
    property int clipId: -1
    property int sceneId: -1
    property int clipStartFrame: 0
    property int clipDurationFrames: 0
    // CompositeView の FB 収集ロジックから参照される公開プロパティ
    // clipNode (CompositeView の delegate) から layer 番号を注入する
    // clipNode.clipLayerRole は model.layer と同期、
    // fbRendererOutput は NodeLoader.onItemChanged で接続される
    property int clipLayerRole: -1
    // NodeLoader.onItemChanged で注入されるレイヤー番号
    property int clipLayer: -1
    // CompositeView の clipNode から直接セット
    // FB 収集対象: 変換済み2Dキャプチャアイテム
    // FB 収集対象: 変換済み2Dキャプチャアイテム (外部から item.fbCaptureItem でアクセス可能)
    // 【修正】3D板ポリ用: 2D変換済の1920x1080ではなく、元サイズの rendererInstance.output を返す
    property alias fbCaptureItem: rendererInstance.output
    property Item fbRendererOutput: rendererInstance.output
    // 座標変換のモジュール化
    // transformエフェクトのモデルを探す
    readonly property var transformModel: {
        for (var i = 0; i < rawEffectModels.length; i++) {
            if (rawEffectModels[i].id === "transform")
                return rawEffectModels[i];

        }
        return null;
    }
    // transformLoader.item (Transform.qmlのインスタンス) が存在するか
    readonly property bool hasTransform: transformLoader.status === Loader.Ready && transformLoader.item
    // transformModelの変更検知用
    property int _tmRev: 0
    // ECS 直接評価キャッシュ（relFrame / _tmRev 変化時に更新）
    property var _ecsParamCache: ({
    })
    // 合成モードの計算 (Transform.qmlを変更できないためここで処理)
    readonly property int blendMode: {
        var m = evalString("transform", "blendMode", qsTr("通常"));
        if (m === qsTr("スクリーン"))
            return DefaultMaterial.Screen;

        if (m === qsTr("乗算"))
            return DefaultMaterial.Multiply;

        if (m === qsTr("オーバーレイ"))
            return DefaultMaterial.Overlay;

        if (m === qsTr("焼き込み"))
            return DefaultMaterial.ColorBurn;

        if (m === qsTr("覆い焼き"))
            return DefaultMaterial.ColorDodge;

        return DefaultMaterial.SourceOver;
    }
    // カリングモード (Transform.qmlから取得)
    readonly property int cullMode: hasTransform ? transformLoader.item.outputCullMode : DefaultMaterial.NoCulling
    // 自動計算プロパティ
    property int currentFrame: 0
    // Will be overridden by CompositeView
    readonly property int relFrame: currentFrame - clipStartFrame
    readonly property real projectFps: (Workspace.currentTimeline && Workspace.currentTimeline.project) ? Workspace.currentTimeline.project.fps : 60
    property var rawEffectModels: []
    // フィルタ系エフェクト（transform/object以外）
    readonly property var filterModels: {
        var res = [];
        for (var i = 0; i < rawEffectModels.length; i++) {
            var eff = rawEffectModels[i];
            if (eff.kind === "effect")
                res.push(eff);

        }
        return res;
    }
    // padding は ObjectRenderer の totalExpansion に委譲したため削除済み
    // 子クラスがオーバーライドするプロパティ
    // 3Dオブジェクト（MeshObject等）はtrue。板ポリ描画をスキップしてオブジェクト自身のModelで描画する
    property bool is3DObject: false
    property Item sourceItem
    property alias renderer: rendererInstance

    function evalParam(effectId, paramName, fallback) {
        var _ = base._tmRev; // リアクティブ依存
        // ECS キャッシュから取得（優先）
        var effMap = base._ecsParamCache[effectId];
        if (effMap !== undefined && effMap[paramName] !== undefined)
            return effMap[paramName];

        // フォールバック: ECS 未同期の瞬間は rawEffectModels の params を参照
        for (var i = 0; i < base.rawEffectModels.length; i++) {
            if (base.rawEffectModels[i].id === effectId && base.rawEffectModels[i].params && base.rawEffectModels[i].params[paramName] !== undefined)
                return base.rawEffectModels[i].params[paramName];

        }
        return fallback;
    }

    // ECS キャッシュを更新する（relFrame・_tmRev・clipId 変化時に呼ぶ）
    function _refreshEcsCache() {
        if (clipId < 0 || !Workspace.currentTimeline)
            return ;

        _ecsParamCache = Workspace.currentTimeline.evaluateClipParams(clipId, relFrame);
    }

    function evalString(effectId, paramName, fallback) {
        var v = evalParam(effectId, paramName, undefined);
        return (v !== undefined && v !== null) ? String(v) : fallback;
    }

    function evalNumber(effectId, paramName, fallback) {
        var v = evalParam(effectId, paramName, undefined);
        return (v !== undefined && v !== null && v !== "") ? Number(v) : fallback;
    }

    function evalBool(effectId, paramName, fallback) {
        var v = evalParam(effectId, paramName, undefined);
        return (v !== undefined && v !== null) ? Boolean(v) : fallback;
    }

    function evalColor(effectId, paramName, fallback) {
        var v = evalParam(effectId, paramName, undefined);
        return (v !== undefined && v !== null) ? v : fallback;
    }

    function adopt2D(item) {
        if (!item || !renderHost)
            return ;

        if (item.parent === renderHost)
            return ;

        item.parent = renderHost;
        console.log("[BASE] adopt2D: item=" + item + " -> renderHost=" + renderHost + " item.w=" + item.width + " item.h=" + item.height);
        // visible を落とすと SceneGraph から外れてテクスチャ更新が止まり得るので触らない。
        // 表示は CompositeView 側の host opacity と ShaderEffectSource.hideSource に任せる。
        owned2D.push(item);
    }

    onClipIdChanged: {
        if (clipId >= 0 && Workspace.currentTimeline) {
            rawEffectModels = Workspace.currentTimeline.getClipEffectsModel(clipId);
            Qt.callLater(_refreshEcsCache);
        }
    }
    // NodeのプロパティをtransformModelにバインド
    position: hasTransform ? transformLoader.item.outputPosition : Qt.vector3d(0, 0, 0)
    eulerRotation: hasTransform ? transformLoader.item.outputRotation : Qt.vector3d(0, 0, 0)
    pivot: hasTransform ? transformLoader.item.outputPivot : Qt.vector3d(0, 0, 0)
    scale: Qt.vector3d(1, 1, 1) // 下のModelで個別に設定
    // renderHost が後からセットされても確実に移送する
    onRenderHostChanged: {
        Qt.callLater(function() {
            adopt2D(base.sourceItem);
            adopt2D(rendererInstance);
        });
    }
    Component.onCompleted: {
        Qt.callLater(function() {
            // 各オブジェクト(TextObject/RectObject)が set してくる sourceItem を移す
            adopt2D(base.sourceItem);
            // ObjectRenderer(= ShaderEffectSource/effectsチェーン)も移す
            adopt2D(rendererInstance);
        });
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
    // sourceItem の visible/opacity は BaseObject が一元管理する。各オブジェクト側で visible を指定しないこと。
    onSourceItemChanged: {
        if (sourceItem) {
            // 【修正】adopt2D を即時実行する。
            // 以前は width > 0 を待つ Timer を使っていたが、TextObject 等では
            // renderHost に reparent されるまで implicitWidth が 0 のまま（鶏卵問題）。
            // ObjectRenderer の textureSource が width <= 0 のとき fallbackItem を
            // 返す設計で既に保護されているため、ここでの遅延は不要かつ有害。
            sourceItem.visible = true;
            sourceItem.opacity = 1;
            Qt.callLater(function() {
                adopt2D(base.sourceItem);
            });
        }
    }
    onRelFrameChanged: {
        if (hasTransform)
            transformLoader.item.frame = relFrame;

        Qt.callLater(_refreshEcsCache);
    }

    Instantiator {
        model: base.rawEffectModels

        Connections {
            function onParamsChanged() {
                base._tmRev++;
            }

            function onKeyframeTracksChanged() {
                base._tmRev++;
            }

            target: modelData
            ignoreUnknownSignals: true
        }

    }

    // transformエフェクトのインスタンス化
    Loader {
        id: transformLoader

        source: (base.transformModel && base.transformModel.qmlSource) ? base.transformModel.qmlSource : ""
        // BaseEffectのプロパティを注入
        onLoaded: {
            item.source = null; // Transformはsourceを持たない
            item.params = base.transformModel.params;
            item.effectModel = base.transformModel;
            item.frame = base.relFrame;
        }
    }

    Connections {
        function onParamsChanged() {
            _tmRev++;
        }

        function onKeyframeTracksChanged() {
            _tmRev++;
        }

        target: transformModel
        ignoreUnknownSignals: true
    }

    Connections {
        function onClipEffectsChanged(changedClipId) {
            if (changedClipId === clipId)
                rawEffectModels = Workspace.currentTimeline.getClipEffectsModel(clipId);

        }

        function onClipsChanged() {
            rawEffectModels = Workspace.currentTimeline.getClipEffectsModel(clipId);
        }

        // [BUG FIX #2] パラメーター変更 → プレビュー反映。
        // effectParamChanged は ECS にしか書き込まないため EffectModel* の params は更新されず
        // onParamsChanged が発火しない。_tmRev を increment して evalParam の依存グラフを
        // invalidate し、ObjectRenderer が再評価されてプレビューが更新される。
        function onEffectParamChanged(changedClipId, effectIndex, paramName, value) {
            if (changedClipId === clipId) {
                Qt.callLater(base._refreshEcsCache);
                base._tmRev++;
            }
        }

        target: Workspace.currentTimeline
    }

    // レンダラー自動配置
    Common.ObjectRenderer {
        id: rendererInstance

        originalSource: base.sourceItem
        onOriginalSourceChanged: console.log("[BASE] ObjectRenderer.originalSource changed=" + originalSource + " w=" + (originalSource ? originalSource.width : -1) + " h=" + (originalSource ? originalSource.height : -1))
        effectModels: base.filterModels
        relFrame: base.relFrame
        clipEvalParams: base._ecsParamCache
    }

    sourceItem: Item {
        // デフォルトのダミー sourceItem。visible/opacity は onSourceItemChanged で BaseObject が設定する。
        width: 1
        height: 1
    }

}
