import "." as Common
import QtQuick
import QtQuick3D

Node {
    // object エフェクト（text/rect/image 等）のパラメータ変更検知
    // _tmRev と同じカウンタ方式: property var 配列要素への直接依存は
    // QML エンジンが追跡できないため Connections 経由で強制通知する

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
    property alias fbCaptureItem: _fbCaptureItemImpl
    property Item fbRendererOutput: _fbCaptureItemImpl
    // --- 座標変換のモジュール化 ---
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
    readonly property real projectFps: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.fps : 60
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
    readonly property real padding: getBlurPadding()
    // 子クラスがオーバーライドするプロパティ
    property Item sourceItem
    property alias renderer: rendererInstance
    // 【統一API】キーフレーム優先評価（全オブジェクトで使用可能）
    // _paramRev を読むことで Connections→onParamsChanged() への依存を確立する。
    // property var の配列要素に対する直接依存は QML エンジンが追跡できないため、
    // _tmRev と同じカウンタ方式を採用する。
    // CompositeViewのNodeLoaderから注入される評価済みパラメータ (単一ソース)
    property var clipEvalParams: ({
    })
    readonly property var evalParams: clipEvalParams ?? ({
    })

    function _lookupParam(effectId, paramName) {
        var eff = evalParams ? evalParams[effectId] : undefined;
        return eff ? eff[paramName] : undefined;
    }

    function evalParam(effectId, paramName, fallback) {
        var v = _lookupParam(effectId, paramName);
        return (v !== undefined && v !== null) ? v : fallback;
    }

    function evalString(effectId, paramName, fallback) {
        var v = _lookupParam(effectId, paramName);
        return (v !== undefined && v !== null) ? String(v) : fallback;
    }

    function evalNumber(effectId, paramName, fallback) {
        var v = _lookupParam(effectId, paramName);
        return (v !== undefined && v !== null && v !== "") ? Number(v) : fallback;
    }

    function evalBool(effectId, paramName, fallback) {
        var v = _lookupParam(effectId, paramName);
        return (v !== undefined && v !== null) ? Boolean(v) : fallback;
    }

    function evalColor(effectId, paramName, fallback) {
        var v = _lookupParam(effectId, paramName);
        return (v !== undefined && v !== null) ? v : fallback;
    }

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

    // ぼかしパディング自動計算（全オブジェクト共通）
    function getBlurPadding() {
        for (let i = 0; i < rawEffectModels.length; i++) {
            if ((rawEffectModels[i].id === "blur" || rawEffectModels[i].id === "border_blur" || rawEffectModels[i].id === "glow" || rawEffectModels[i].id === "flash" || rawEffectModels[i].id === "diffuse_light") && rawEffectModels[i].enabled) {
                var v = rawEffectModels[i].evaluatedParam ? rawEffectModels[i].evaluatedParam("size", relFrame, projectFps) : undefined;
                if (v === undefined || v === null)
                    v = rawEffectModels[i].evaluatedParam ? rawEffectModels[i].evaluatedParam("diffusion", relFrame, projectFps) : undefined;

                if (v === undefined || v === null)
                    v = rawEffectModels[i].evaluatedParam ? rawEffectModels[i].evaluatedParam("strength", relFrame, projectFps) : undefined;

                if (v === undefined || v === null)
                    v = rawEffectModels[i].params["size"] || rawEffectModels[i].params["diffusion"] || rawEffectModels[i].params["strength"];

                // FastBlurの特性上、半径の3倍程度の余白がないと端が切れて不自然になるため広めに確保する
                return Number(v || 0) * 3;
            }
        }
        return 0;
    }

    // NodeのプロパティをtransformModelにバインド
    position: hasTransform ? transformLoader.item.outputPosition : Qt.vector3d(0, 0, 0)
    eulerRotation: hasTransform ? transformLoader.item.outputRotation : Qt.vector3d(0, 0, 0)
    pivot: hasTransform ? transformLoader.item.outputPivot : Qt.vector3d(0, 0, 0)
    scale: Qt.vector3d(1, 1, 1) // 下のModelで個別に設定
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
    // sourceItem は常に非表示（renderer.output のみ表示）
    onSourceItemChanged: {
        if (sourceItem) {
            // キャプチャ安定化のためvisibleは落とさず、不可視化はopacityで行う
            sourceItem.visible = true;
            sourceItem.opacity = 1;
        }
    }
    onRelFrameChanged: {
        if (hasTransform)
            transformLoader.item.frame = relFrame;

    }

    // --- transformエフェクトのインスタンス化 ---
    Loader {
        id: transformLoader

        source: (root.transformModel && root.transformModel.qmlSource) ? root.transformModel.qmlSource : ""
        // BaseEffectのプロパティを注入
        onLoaded: {
            item.source = null; // Transformはsourceを持たない
            item.params = root.transformModel.params;
            item.effectModel = root.transformModel;
            item.frame = root.relFrame;
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
                rawEffectModels = TimelineBridge.getClipEffectsModel(clipId);

        }

        function onClipsChanged() {
            rawEffectModels = TimelineBridge.getClipEffectsModel(clipId);
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

            // Transform.qmlのインスタンスから値を取得
            readonly property var _ti: base.hasTransform ? transformLoader.item : null

            // テクスチャサイズをスケール適用後のサイズに設定
            width: (rendererInstance && rendererInstance.output && rendererInstance.output.sourceItem ? rendererInstance.output.sourceItem.width : 1) * (_ti ? _ti.output2dScale : 1)
            height: (rendererInstance && rendererInstance.output && rendererInstance.output.sourceItem ? rendererInstance.output.sourceItem.height : 1) * (_ti ? _ti.output2dScale : 1)
            // AviUtl 座標系: 中心(0,0)、Y下プラス → Qt2D: 中心 = parent の center + offset
            x: _fbCaptureItemImpl.width / 2 + (_ti ? _ti.output2dX : 0) - width / 2
            y: _fbCaptureItemImpl.height / 2 - (_ti ? _ti.output2dY : 0) - height / 2
            rotation: -(_ti ? _ti.output2dRotationZ : 0)
            opacity: _ti ? _ti.outputOpacity : 1

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
