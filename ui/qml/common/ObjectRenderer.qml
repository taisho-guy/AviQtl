import QtQuick
import QtQuick.Effects

Item {
    id: renderer
    
    // 入力: 元となる映像ソース（TextやRectangleなど）
    required property Item originalSource
    
    // 入力: 適用するエフェクトのリスト (filterModels)
    required property list<QtObject> effectModels
    required property int relFrame

    // フォールバック用ダミー（確実に有効な Item）
    width: originalSource ? originalSource.width : 1
    height: originalSource ? originalSource.height : 1

    Item {
        id: fallbackItem
        width: 1
        height: 1
        visible: true   // テクスチャ化のために可視である必要がある
        opacity: 0.0    // ただし見えないようにする
    }

    // 出力: 最終的にレンダリングされたテクスチャソース（Modelで使用）
    property alias output: textureSource

    // 内部: チェーンの構築
    Repeater {
        id: effectChain
        model: renderer.effectModels

        Loader {
            id: effectLoader
            active: modelData.enabled
            source: modelData.qmlSource
            
            // 前段の出力を取得 (index 0 なら originalSource)
            property Item inputSource: {
                if (index === 0) return renderer.originalSource;
                var prevLoader = effectChain.itemAt(index - 1);
                if (prevLoader && prevLoader.status === Loader.Ready) {
                    return prevLoader.item;
                }
                return renderer.originalSource; // Fallback
            }

            // 【重要】宣言的Bindingによる堅牢な同期
            Binding { target: effectLoader.item; property: "source"; value: effectLoader.inputSource; when: effectLoader.status === Loader.Ready }
            Binding { target: effectLoader.item; property: "params"; value: modelData.params; when: effectLoader.status === Loader.Ready }
            Binding { target: effectLoader.item; property: "effectModel"; value: modelData; when: effectLoader.status === Loader.Ready }
            // キーフレーム評価用（全エフェクト共通）
            Binding { target: effectLoader.item; property: "frame"; value: renderer.relFrame; when: effectLoader.status === Loader.Ready }
            
            // サイズ同期: 前段のサイズを引き継ぐ（エフェクト側で拡張も可能）
            Binding { target: effectLoader.item; property: "width"; value: effectLoader.inputSource.width; when: effectLoader.status === Loader.Ready }
            Binding { target: effectLoader.item; property: "height"; value: effectLoader.inputSource.height; when: effectLoader.status === Loader.Ready }
        }
    }

    // 最終段の出力
    ShaderEffectSource {
        id: textureSource
        sourceItem: {
            // originalSource が無効な場合は fallbackItem を使用
            if (!renderer.originalSource || renderer.originalSource.width <= 0 || renderer.originalSource.height <= 0) 
                return fallbackItem;
            if (effectChain.count > 0) return findFinalOutput(renderer.originalSource);
            return renderer.originalSource;
        }

        // 2D側への描画を確実に止める（3DのTexture.sourceItemとしては機能する）
        visible: false

        function findFinalOutput(defaultSource) {
            var count = effectChain.count;
            for (var i = count - 1; i >= 0; i--) {
                var loader = effectChain.itemAt(i);
                if (loader) {
                    if (loader.status === Loader.Ready && loader.active && loader.item && loader.item.width > 0 && loader.item.height > 0) {
                        return loader.item;
                    }
                }
            }
            return defaultSource;
        }

        // 念のため：出力更新を止めない（既定でtrueだが明示）
        live: true
        hideSource: true
        recursive: false
    }
}