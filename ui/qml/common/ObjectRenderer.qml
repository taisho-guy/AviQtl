import QtQuick

Item {
    id: renderer

    required property Item originalSource
    required property var effectModels
    required property int relFrame
    property var clipEvalParams: ({
    })
    property int _paramRev: 0
    property alias output: textureSource
    readonly property Item finalItem: textureSource.sourceItem
    // ─── 自動キャンバス拡張 ──────────────────────────────────────
    // 全エフェクトの expansion を集計して描画キャンバスを動的に決定する。
    // sourceItem のコンテンツサイズ + 全エフェクトが要求する余白 = 実際の描画領域。
    readonly property var totalExpansion: {
        var t = 0, r = 0, b = 0, l = 0;
        for (var i = 0; i < effectChain.count; i++) {
            var loader = effectChain.itemAt(i);
            if (loader && loader.status === Loader.Ready && loader.active && loader.item && "expansion" in loader.item) {
                var ex = loader.item.expansion;
                t += (ex.top || 0);
                r += (ex.right || 0);
                b += (ex.bottom || 0);
                l += (ex.left || 0);
            }
        }
        return {
            "top": t,
            "right": r,
            "bottom": b,
            "left": l
        };
    }
    // コンテンツ本来のサイズ（拡張前）
    // width が 0 の場合（Node 内で初期化された Item 等）は implicitWidth にフォールバックする
    readonly property real contentWidth: originalSource ? Math.max(originalSource.width > 0 ? originalSource.width : (originalSource.implicitWidth > 0 ? originalSource.implicitWidth : 1), 1) : 1
    readonly property real contentHeight: originalSource ? Math.max(originalSource.height > 0 ? originalSource.height : (originalSource.implicitHeight > 0 ? originalSource.implicitHeight : 1), 1) : 1
    // ─── コンテンツオフセット ────────────────────────────────────
    // 拡張余白の分だけ originalSource のレンダリング開始位置をずらす。
    // 各エフェクトは anchors.fill: parent により拡張済みキャンバス全体を受け取る。
    readonly property real contentX: totalExpansion.left
    readonly property real contentY: totalExpansion.top

    onClipEvalParamsChanged: _paramRev++
    // 拡張済みキャンバスサイズ（エフェクト込み）
    width: contentWidth + totalExpansion.left + totalExpansion.right
    height: contentHeight + totalExpansion.top + totalExpansion.bottom

    Item {
        id: fallbackItem

        width: 1
        height: 1
        visible: true
        opacity: 1 // 【修正】3D Texture キャプチャのため実体は不透明にする
    }

    // originalSource をオフセット付きでキャンバス内に配置するプロキシ
    // エフェクトチェーンはこのプロキシをソースとして受け取る
    Item {
        id: contentProxy

        width: renderer.width
        height: renderer.height
        visible: true
        opacity: 1 // 【修正】3D Texture キャプチャのため実体は不透明にする

        // originalSource 自体は ShaderEffectSource でテクスチャ化してオフセット配置する
        ShaderEffectSource {
            x: renderer.contentX
            y: renderer.contentY
            width: renderer.contentWidth
            height: renderer.contentHeight
            sourceItem: renderer.originalSource
            live: true
            hideSource: false
            visible: true
        }

    }

    Repeater {
        id: effectChain

        model: renderer.effectModels

        Loader {
            id: effectLoader

            property Item inputSource: {
                if (index === 0)
                    return contentProxy;

                var prev = effectChain.itemAt(index - 1);
                if (prev && prev.status === Loader.Ready)
                    return prev.item;

                return contentProxy;
            }

            function _syncAll() {
                if (!item)
                    return ;

                if ("source" in item)
                    item.source = inputSource;

                if ("params" in item)
                    item.params = Qt.binding(function() {
                    var ep = renderer.clipEvalParams[modelData.id];
                    return ep !== undefined ? ep : modelData.params;
                });

                if ("effectModel" in item)
                    item.effectModel = modelData;

                if ("frame" in item)
                    item.frame = renderer.relFrame;

                if ("clipEvalParams" in item)
                    item.clipEvalParams = Qt.binding(function() {
                    return renderer.clipEvalParams;
                });

                if ("_paramRev" in item)
                    item._paramRev = Qt.binding(function() {
                    return renderer._paramRev;
                });

            }

            // 全エフェクトは拡張済みキャンバス全体を受け取る
            anchors.fill: parent
            active: modelData.enabled
            source: modelData.qmlSource
            onLoaded: _syncAll()
            onInputSourceChanged: {
                if (status === Loader.Ready && "source" in item)
                    item.source = inputSource;

            }

            Connections {
                function onRelFrameChanged() {
                    if (effectLoader.status === Loader.Ready && effectLoader.item && "frame" in effectLoader.item)
                        effectLoader.item.frame = renderer.relFrame;

                }

                target: renderer
            }

        }

    }

    ShaderEffectSource {
        id: textureSource

        function findFinalOutput(defaultSource) {
            for (var i = effectChain.count - 1; i >= 0; i--) {
                var loader = effectChain.itemAt(i);
                if (loader && loader.status === Loader.Ready && loader.active && loader.item)
                    return loader.item;

            }
            return defaultSource;
        }

        // View3D.Inline モードでは ShaderEffectSource が直接 textureProvider として機能するため
        // layer.enabled は不要（二重 FBO は Vulkan でテクスチャ同期の不具合を引き起こす）
        // 【修正】FBOサイズを拡張済みキャンバスに明示バインドする。
        // サイズ未指定だと fallbackItem(1x1) の間に FBO が 1x1 で固定される。
        width: renderer.width
        height: renderer.height
        sourceItem: {
            // originalSource の null チェックのみ行う
            // width<=0 による fallback は廃止: adopt2D タイミングの問題で
            // sourceItem.width が 0 のまま fallbackItem(1x1) に固定されるのを防ぐ
            // contentWidth は Math.max(w,1) で既に最低1pxが保証されているため安全
            if (!renderer.originalSource)
                return fallbackItem;

            if (effectChain.count > 0)
                return findFinalOutput(contentProxy);

            return contentProxy;
        }
        visible: true
        opacity: 1
        // contentWidth/Height > 1: adopt2D 前の w=0 状態（= 1×1 fallback）では
        // FBO を焼き付けないよう live=false にして保護する。
        // adopt2D 後に originalSource のサイズが確定すると contentWidth > 1 になり
        // live=true に切り替わって FBO が正しいサイズで再生成される。
        live: renderer.contentWidth > 1 && renderer.contentHeight > 1
        hideSource: true
        recursive: false
    }

}
