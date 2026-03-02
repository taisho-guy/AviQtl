import QtQuick

Item {
    id: renderer

    required property Item originalSource
    required property var effectModels
    required property int relFrame
    property alias output: textureSource
    readonly property Item finalItem: textureSource.sourceItem

    width: originalSource ? originalSource.width : 1
    height: originalSource ? originalSource.height : 1

    Item {
        id: fallbackItem

        width: 1
        height: 1
        visible: true
        opacity: 0
    }

    Repeater {
        id: effectChain

        model: renderer.effectModels

        Loader {
            id: effectLoader

            property Item inputSource: {
                if (index === 0)
                    return renderer.originalSource;

                var prev = effectChain.itemAt(index - 1);
                if (prev && prev.status === Loader.Ready)
                    return prev.item;

                return renderer.originalSource;
            }

            function _syncAll() {
                if (!item)
                    return ;

                if ("source" in item)
                    item.source = inputSource;

                if ("params" in item)
                    item.params = modelData.params;

                if ("effectModel" in item)
                    item.effectModel = modelData;

                if ("frame" in item)
                    item.frame = renderer.relFrame;

            }

            // rendererInstance のサイズに追従させ、item (BaseEffect) のサイズを確定させる
            anchors.fill: parent
            active: modelData.enabled
            source: modelData.qmlSource
            onLoaded: _syncAll()
            onInputSourceChanged: {
                if (status === Loader.Ready) {
                    if ("source" in item)
                        item.source = inputSource;

                }
            }

            Connections {
                function onRelFrameChanged() {
                    if (effectLoader.status === Loader.Ready && effectLoader.item && "frame" in effectLoader.item)
                        effectLoader.item.frame = renderer.relFrame;

                }

                target: renderer
            }

            Connections {
                function onParamsChanged() {
                    if (effectLoader.status === Loader.Ready && effectLoader.item && "params" in effectLoader.item)
                        effectLoader.item.params = modelData.params;

                }

                target: modelData
            }

        }

    }

    ShaderEffectSource {
        id: textureSource

        function findFinalOutput(defaultSource) {
            for (var i = effectChain.count - 1; i >= 0; i--) {
                var loader = effectChain.itemAt(i);
                // width/height チェックを除去: anchors.fill: parent で親に追従するため
                // サイズが 0 の場合は item が存在することだけを確認する
                if (loader && loader.status === Loader.Ready && loader.active && loader.item)
                    return loader.item;

            }
            return defaultSource;
        }

        sourceItem: {
            if (!renderer.originalSource || renderer.originalSource.width <= 0 || renderer.originalSource.height <= 0)
                return fallbackItem;

            if (effectChain.count > 0)
                return findFinalOutput(renderer.originalSource);

            return renderer.originalSource;
        }
        visible: false
        live: true
        hideSource: true
        recursive: false
    }

}
