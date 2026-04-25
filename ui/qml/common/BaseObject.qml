import "." as Common
import QtQuick
import QtQuick3D

Node {
    // ─────────────────────────────────────────────────────────────────
    // ECS 評価済みパラメーターキャッシュ（唯一の真実）
    // 形式: { effectId: { paramName: value, ... }, ... }
    // _cacheRev（整数）を明示的な依存として持つことで確実な再評価を保証する。
    // 設計根拠:
    //   QML の binding は C++ Q_INVOKABLE の戻り値変化を自動追跡できない。
    //   property var への新オブジェクト代入は参照比較で変化を検出するが、
    //   C++ QVariantMap 変換オブジェクトの参照比較は不安定になる場合がある。
    //   int プリミティブのインクリメントは常に確実に変化を通知するため、
    //   _cacheRev を介して ECS 状態変化を binding に伝達する。
    // ecsCache binding は relFrame を直接依存に持つため追加トリガー不要。
    // relFrame 変化 → ecsCache binding 自動再評価。

    id: base

    property Item renderHost: null
    property var owned2D: []
    property int clipId: -1
    property int sceneId: -1
    property int clipStartFrame: 0
    property int clipDurationFrames: 0
    property int clipLayerRole: -1
    property int clipLayer: -1
    property alias fbCaptureItem: rendererInstance.output
    property Item fbRendererOutput: rendererInstance.output
    readonly property var transformModel: {
        for (var i = 0; i < rawEffectModels.length; i++) {
            if (rawEffectModels[i].id === "transform")
                return rawEffectModels[i];

        }
        return null;
    }
    readonly property bool hasTransform: transformLoader.status === Loader.Ready && transformLoader.item
    // 再評価トリガー:
    //   relFrame 変化   → binding が relFrame に直接依存しているため自動
    //   clipId 変化     → binding が clipId に直接依存しているため自動 + Qt.callLater
    //   C++ シグナル起点 → _cacheRev++ で強制再評価
    // ─────────────────────────────────────────────────────────────────
    property int _cacheRev: 0
    readonly property var ecsCache: {
        var _ = _cacheRev; // 整数依存: _cacheRev++ で確実に再評価される
        if (clipId < 0 || !Workspace.currentTimeline)
            return ({
        });

        return Workspace.currentTimeline.evaluateClipParams(clipId, relFrame);
    }
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
    readonly property int cullMode: hasTransform ? transformLoader.item.outputCullMode : DefaultMaterial.NoCulling
    property int currentFrame: 0
    readonly property int relFrame: currentFrame - clipStartFrame
    readonly property real projectFps: (Workspace.currentTimeline && Workspace.currentTimeline.project) ? Workspace.currentTimeline.project.fps : 60
    property var rawEffectModels: []
    readonly property var filterModels: {
        var res = [];
        for (var i = 0; i < rawEffectModels.length; i++) {
            if (rawEffectModels[i].kind === "effect")
                res.push(rawEffectModels[i]);

        }
        return res;
    }
    property bool is3DObject: false
    property Item sourceItem
    property alias renderer: rendererInstance

    // 【統一API】ecsCache を直接参照する。
    // ecsCache が再評価されると、evalParam を呼び出している全 binding も
    // ecsCache への依存経由で自動的に再評価される。
    function evalParam(effectId, paramName, fallback) {
        var m = ecsCache[effectId];
        if (m !== undefined && m[paramName] !== undefined && m[paramName] !== null)
            return m[paramName];

        return fallback;
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
        owned2D.push(item);
    }

    onClipIdChanged: {
        if (clipId >= 0 && Workspace.currentTimeline) {
            rawEffectModels = Workspace.currentTimeline.getClipEffectsMeta(clipId);
            // Qt.callLater で遅延: clipId 変化直後は ECS が commit 前の可能性があるため
            // 次ティックで確実に最新スナップショットを読む。
            // （clipId は binding の直接依存なので即時評価も起きるが、
            //   ECS 未同期の場合 {} が返る。callLater で確実に再評価する）
            Qt.callLater(function() {
                _cacheRev++;
            });
        }
    }
    position: hasTransform ? transformLoader.item.outputPosition : Qt.vector3d(0, 0, 0)
    eulerRotation: hasTransform ? transformLoader.item.outputRotation : Qt.vector3d(0, 0, 0)
    pivot: hasTransform ? transformLoader.item.outputPivot : Qt.vector3d(0, 0, 0)
    scale: Qt.vector3d(1, 1, 1)
    onRenderHostChanged: {
        Qt.callLater(function() {
            adopt2D(base.sourceItem);
            adopt2D(rendererInstance);
        });
    }
    Component.onCompleted: {
        Qt.callLater(function() {
            adopt2D(base.sourceItem);
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
    onSourceItemChanged: {
        if (sourceItem) {
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

    }

    Loader {
        id: transformLoader

        source: (base.transformModel && base.transformModel.qmlSource) ? base.transformModel.qmlSource : ""
        onLoaded: {
            item.source = null;
            item.effectModel = base.transformModel;
            item.frame = base.relFrame;
            // ecsCache["transform"] を params に注入。
            // base.ecsCache が再評価されると Qt.binding が追跡する依存経由で自動再評価。
            if ("params" in item)
                item.params = Qt.binding(function() {
                var ep = base.ecsCache["transform"];
                return ep !== undefined ? ep : {
                };
            });

        }
    }

    Connections {
        // エフェクト構成変化（追加・削除・並び替え）
        function onClipEffectsChanged(changedClipId) {
            if (changedClipId === clipId) {
                rawEffectModels = Workspace.currentTimeline.getClipEffectsMeta(clipId);
                _cacheRev++;
            }
        }

        function onClipsChanged() {
            rawEffectModels = Workspace.currentTimeline.getClipEffectsMeta(clipId);
            _cacheRev++;
        }

        // パラメーター値変化（SettingDialog 操作・Undo/Redo）
        // ECS は既に更新済み。_cacheRev++ で ecsCache binding を再評価させる。
        // 同期実行（Qt.callLater なし）: 操作直後にプレビューへ即時反映するため。
        function onEffectParamChanged(changedClipId, effectIndex, paramName, value) {
            if (changedClipId === clipId)
                _cacheRev++;

        }

        target: Workspace.currentTimeline
    }

    Common.ObjectRenderer {
        id: rendererInstance

        originalSource: base.sourceItem
        onOriginalSourceChanged: console.log("[BASE] ObjectRenderer.originalSource changed=" + originalSource + " w=" + (originalSource ? originalSource.width : -1) + " h=" + (originalSource ? originalSource.height : -1))
        effectModels: base.filterModels
        relFrame: base.relFrame
        clipEvalParams: base.ecsCache
    }

    sourceItem: Item {
        width: 1
        height: 1
    }

}
