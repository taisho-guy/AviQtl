import QtQuick

Item {
    id: renderer

    required property Item originalSource
    required property var effectModels
    required property int relFrame
    // BaseObject から注入される ECS 評価済みキャッシュ（全エフェクト分）
    property var clipEvalParams: ({
    })
    property alias output: textureSource
    readonly property Item finalItem: textureSource.sourceItem
    // 全エフェクトの expansion を集計して描画キャンバスを動的に決定する
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
    readonly property real contentWidth: originalSource ? Math.max(originalSource.width > 0 ? originalSource.width : (originalSource.implicitWidth > 0 ? originalSource.implicitWidth : 1), 1) : 1
    readonly property real contentHeight: originalSource ? Math.max(originalSource.height > 0 ? originalSource.height : (originalSource.implicitHeight > 0 ? originalSource.implicitHeight : 1), 1) : 1
    readonly property real contentX: totalExpansion.left
    readonly property real contentY: totalExpansion.top

    width: contentWidth + totalExpansion.left + totalExpansion.right
    height: contentHeight + totalExpansion.top + totalExpansion.bottom

    Item {
        id: fallbackItem

        width: 1
        height: 1
        visible: true
        opacity: 1
    }

    Item {
        id: contentProxy

        width: renderer.width
        height: renderer.height
        visible: true
        opacity: 1

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

                if ("effectModel" in item)
                    item.effectModel = modelData;

                if ("frame" in item)
                    item.frame = renderer.relFrame;

            }

            anchors.fill: parent
            active: modelData.enabled
            source: modelData.qmlSource
            onLoaded: _syncAll()
            onInputSourceChanged: {
                if (status === Loader.Ready && "source" in item)
                    item.source = inputSource;

            }

            // ECS キャッシュを宣言的バインド（実行時 "in" チェック不要）
            // BaseEffect.evalParam は item.ecsCache[effectModel.id] を参照するため全キャッシュを渡す
            Binding {
                target: effectLoader.item
                property: "ecsCache"
                value: renderer.clipEvalParams
                when: effectLoader.status === Loader.Ready && effectLoader.item !== null
                restoreMode: Binding.RestoreNone
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

        width: renderer.width
        height: renderer.height
        sourceItem: {
            if (!renderer.originalSource)
                return fallbackItem;

            if (effectChain.count > 0)
                return findFinalOutput(contentProxy);

            return contentProxy;
        }
        visible: true
        opacity: 1
        live: renderer.contentWidth > 1 && renderer.contentHeight > 1
        hideSource: true
        recursive: false
    }

}
