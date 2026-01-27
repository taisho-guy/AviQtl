import QtQuick
import QtQml

Item {
    id: root
    property var sourceItem: null
    property var effectModels: []

    ShaderEffectSource {
        id: sourceProxy
        sourceItem: root.sourceItem
        visible: false
        live: true
        recursive: false
    }

    Instantiator {
        id: pipeline
        model: root.effectModels
        
        delegate: Loader {
            id: stageLoader
            property var inputSource: index === 0 ? sourceProxy : pipeline.objectAt(index - 1).outputTexture
            
            active: modelData.qmlSource !== "" && modelData.enabled
            source: modelData.qmlSource
            
            property var outputTexture: stageOutput
            
            ShaderEffectSource {
                id: stageOutput
                sourceItem: stageLoader.item
                live: true
                hideSource: true
            }
            
            onLoaded: {
                // コンテナではなく、実態のあるItemとして管理（hideSourceが隠してくれる）
                item.parent = root
                item.width = Qt.binding(() => root.width)
                item.height = Qt.binding(() => root.height)
                item.params = modelData.params
                if (item.hasOwnProperty("source")) {
                    item.source = Qt.binding(() => stageLoader.inputSource)
                }
                // Loader直下のItemはvisible=trueである必要がある（ShaderEffectSourceがキャプチャするため）
                item.visible = true
            }
        }
    }
    
    ShaderEffectSource {
        id: finalOutput
        anchors.fill: parent
        sourceItem: {
            if (pipeline.count === 0) return sourceProxy
            var lastStage = pipeline.objectAt(pipeline.count - 1)
            return lastStage ? lastStage.outputTexture : sourceProxy
        }
        visible: true
        live: true
    }
}