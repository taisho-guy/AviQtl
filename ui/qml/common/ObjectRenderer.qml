import QtQuick
import QtQuick.Effects

Item {
    id: renderer

    required property Item originalSource
    required property list<QtObject> effectModels
    required property int relFrame

    width:  originalSource ? originalSource.width  : 1
    height: originalSource ? originalSource.height : 1

    Item {
        id: fallbackItem
        width: 1; height: 1
        visible: true; opacity: 0.0
    }

    property alias output: textureSource
    readonly property Item finalItem: textureSource.sourceItem

    Repeater {
        id: effectChain
        model: renderer.effectModels

        Loader {
            id: effectLoader
            active: modelData.enabled
            source: modelData.qmlSource

            property Item inputSource: {
                if (index === 0) return renderer.originalSource;
                var prev = effectChain.itemAt(index - 1);
                if (prev && prev.status === Loader.Ready) return prev.item;
                return renderer.originalSource;
            }

            // ─── 命令的同期 (Binding.when ループを回避) ──────────────
            function _syncAll() {
                if (!item) return;
                // "in" 演算子で宣言プロパティの存在を確認してからセット
                if ("source"      in item) item.source      = inputSource;
                if ("params"      in item) item.params      = modelData.params;
                if ("effectModel" in item) item.effectModel = modelData;
                if ("frame"       in item) item.frame       = renderer.relFrame;
            }

            onLoaded:         _syncAll()
            onInputSourceChanged: {
                if (status === Loader.Ready) {
                    if ("source" in item) item.source = inputSource;
                }
            }

            // ソースのサイズ変更を動的に追従させるバインディング
            Binding {
                target: effectLoader.item
                property: "width"
                value: effectLoader.inputSource ? effectLoader.inputSource.width : 0
                when: effectLoader.status === Loader.Ready && effectLoader.item && ("width" in effectLoader.item)
            }
            Binding {
                target: effectLoader.item
                property: "height"
                value: effectLoader.inputSource ? effectLoader.inputSource.height : 0
                when: effectLoader.status === Loader.Ready && effectLoader.item && ("height" in effectLoader.item)
            }

            // relFrame 変化をエフェクトに伝播
            Connections {
                target: renderer
                function onRelFrameChanged() {
                    if (effectLoader.status === Loader.Ready && effectLoader.item
                            && "frame" in effectLoader.item)
                        effectLoader.item.frame = renderer.relFrame;
                }
            }

            // modelData.params 変化をエフェクトに伝播
            Connections {
                target: modelData
                function onParamsChanged() {
                    if (effectLoader.status === Loader.Ready && effectLoader.item
                            && "params" in effectLoader.item)
                        effectLoader.item.params = modelData.params;
                }
            }
        }
    }

    ShaderEffectSource {
        id: textureSource
        sourceItem: {
            if (!renderer.originalSource
                    || renderer.originalSource.width  <= 0
                    || renderer.originalSource.height <= 0)
                return fallbackItem;
            if (effectChain.count > 0)
                return findFinalOutput(renderer.originalSource);
            return renderer.originalSource;
        }
        visible:   false
        live:      true
        hideSource: true
        recursive: false

        function findFinalOutput(defaultSource) {
            for (var i = effectChain.count - 1; i >= 0; i--) {
                var loader = effectChain.itemAt(i);
                if (loader && loader.status === Loader.Ready
                        && loader.active && loader.item
                        && loader.item.width > 0 && loader.item.height > 0)
                    return loader.item;
            }
            return defaultSource;
        }
    }
}
