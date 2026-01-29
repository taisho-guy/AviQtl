import QtQuick
import QtQuick.Effects

Item {
    id: renderer
    
    // 入力: 元となる映像ソース（TextやRectangleなど）
    required property Item originalSource
    
    // 入力: 適用するエフェクトのリスト (filterModels)
    required property var effectModels

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
            
            // サイズ同期: 前段のサイズを引き継ぐ（エフェクト側で拡張も可能）
            Binding { target: effectLoader.item; property: "width"; value: effectLoader.inputSource.width; when: effectLoader.status === Loader.Ready }
            Binding { target: effectLoader.item; property: "height"; value: effectLoader.inputSource.height; when: effectLoader.status === Loader.Ready }
        }
    }

    // 最終段の出力
    ShaderEffectSource {
        id: textureSource
        
        // チェーンの末尾を探す
        sourceItem: {
            var count = effectChain.count;
            for (var i = count - 1; i >= 0; i--) {
                var loader = effectChain.itemAt(i);
                if (loader && loader.status === Loader.Ready && loader.active) {
                    return loader.item;
                }
            }
            return renderer.originalSource;
        }

        hideSource: true
        live: true // チェーン処理には live: true が必要
        visible: true 
        recursive: false
    }
}