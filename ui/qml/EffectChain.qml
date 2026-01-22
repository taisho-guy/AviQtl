import QtQuick
import QtQml 

// エフェクトを数珠繋ぎにするコンテナ
// 全てのエフェクトはLoader経由でインスタンス化される
Item {
    id: root
    property var effectModels: []
    property Item sourceItem
    
    width: root.sourceItem ? root.sourceItem.width : 0
    height: root.sourceItem ? root.sourceItem.height : 0

    // パイプラインの始点（ソース画像）
    // シェーダーチェーンの最初の入力として機能する
    Item {
        id: originalSourceWrapper
        width: root.width
        height: root.height
        visible: false 
        
        ShaderEffectSource {
            id: srcProxy
            sourceItem: root.sourceItem
            anchors.fill: parent
            visible: true
            live: true
            recursive: true // 再帰的な描画を許可
        }
    }

    Instantiator {
        id: pipeline
        model: root.effectModels

        delegate: Loader {
            id: loader
            active: modelData.qmlSource !== "" && modelData.enabled
            source: modelData.qmlSource
            
            onLoaded: {
                item.parent = root
                item.anchors.fill = root
                item.params = modelData.params
                root.updatePipeline()
            }
            onActiveChanged: root.updatePipeline()
        }
        onObjectAdded: root.updatePipeline()
        onObjectRemoved: root.updatePipeline()
    }

    function updatePipeline() {
        var currentSource = srcProxy; // 初期入力
        var hasActive = false;
        
        // リセット
        originalSourceWrapper.opacity = 1.0;

        for (var i = 0; i < pipeline.count; i++) {
            var loader = pipeline.objectAt(i);
            if (loader && loader.active && loader.status === Loader.Ready && loader.item) {
                // 前段の出力を入力にする
                if (loader.item.hasOwnProperty("source")) {
                    loader.item.source = currentSource;
                }
                
                // 前段を隠す
                if (currentSource === srcProxy || currentSource === originalSourceWrapper) {
                    originalSourceWrapper.opacity = 0.0;
                } else {
                    // Loaderで生成されたエフェクトはvisible制御でも問題ない場合が多いが
                    // 安全のためここもopacity制御にするか、あるいはvisible制御を維持
                    if (currentSource.visible !== undefined) currentSource.visible = false;
                }
                
                // 自身を表示
                loader.item.visible = true;
                
                currentSource = loader.item;
                hasActive = true;
            }
        }
        
        if (!hasActive) {
            originalSourceWrapper.opacity = 1.0;
        }
    }
}