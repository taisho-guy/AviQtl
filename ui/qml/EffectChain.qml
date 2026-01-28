import QtQuick
import QtQuick.Effects

Item {
    id: root
    property Item sourceItem
    property var effectModels: []

    // 外部参照用プロパティ
    readonly property Item outputItem: {
        var lastIdx = lastActiveIndex();
        if (lastIdx < 0) return root.sourceItem;
        var loader = pipeline.objectAt(lastIdx);
        return loader ? loader.item : root.sourceItem;
    }

    // このItem自体のサイズはソースに合わせる
    implicitWidth: sourceItem ? sourceItem.width : 0
    implicitHeight: sourceItem ? sourceItem.height : 0

    // パイプライン構築
    Instantiator {
        id: pipeline
        model: root.effectModels

        delegate: Loader {
            id: stageLoader
            // 親をrootにすることでサイズ追従を簡単にする
            // parent: root // Instantiatorのdelegate内なので自動的に管理されるが、明示的な配置が必要なら anchors.fill: parent
            anchors.fill: parent
            
            // ソースの連鎖ロジック
            // index 0: root.sourceItem を参照
            // index N: 前のステージの item を参照
            property Item inputSource: {
                if (index === 0) return root.sourceItem;
                var prevLoader = pipeline.objectAt(index - 1);
                return prevLoader ? prevLoader.item : null;
            }
            
            active: modelData && modelData.enabled && modelData.qmlSource !== ""
            source: modelData.qmlSource

            onLoaded: {
                // パラメータ伝達
                item.params = modelData.params;
                if (item.hasOwnProperty("source")) {
                    item.source = Qt.binding(() => stageLoader.inputSource);
                }
                
                // 【重要】レンダリングパイプライン維持のため常に表示する
                item.visible = true;
            }
        }
    }

    function lastActiveIndex() {
        if (!effectModels) return -1;
        for (var i = effectModels.length - 1; i >= 0; i--) {
            var m = effectModels[i];
            if (m && m.enabled && m.qmlSource) return i;
        }
        return -1;
    }

    // エフェクトなし時のパススルー
    MultiEffect {
        anchors.fill: parent
        source: root.sourceItem
        visible: root.lastActiveIndex() < 0
    }
}