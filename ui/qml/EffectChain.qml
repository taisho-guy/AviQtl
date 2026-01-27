import QtQuick
import QtQml 

Item {
    id: root
    property var effectModels: []
    property Item sourceItem
    
    width: root.sourceItem ? root.sourceItem.width : 0
    height: root.sourceItem ? root.sourceItem.height : 0

    // 1. ソースをラップするProxy
    ShaderEffectSource {
        id: sourceProxy
        sourceItem: root.sourceItem
        // EffectChain自体が表示されるため、Sourceは隠すが、Textureは更新し続ける
        visible: false 
        live: true
        recursive: false
    }

    // 2. エフェクトチェーンの動的生成
    Instantiator {
        id: pipeline
        model: root.effectModels

        delegate: Loader {
            // 直前の要素（またはソース）を入力とする
            property var inputSource: index === 0 ? sourceProxy : pipeline.objectAt(index - 1).item
            
            active: modelData.qmlSource !== "" && modelData.enabled
            source: modelData.qmlSource
            
            onLoaded: {
                item.parent = root
                item.anchors.fill = root
                item.params = modelData.params
                
                // 宣言的バインディング: 前段の出力を自身の入力に接続
                if (item.hasOwnProperty("source")) {
                    item.source = Qt.binding(() => inputSource)
                }
            }
        }
    }
}