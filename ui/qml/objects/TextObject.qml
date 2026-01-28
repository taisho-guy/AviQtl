import QtQuick
import QtQuick3D
import QtQuick.Effects
import Rina

Node {
    id: root
    property string textContent: "Text"
    
    // CompositeView(model.params=evaluatedParams) から渡される前提に統一
    property int textSize: 64
    property color color: "#ffffff"
    property real opacity: 1.0
    property int clipId: -1

    // 修正: TimelineBridge.clipsに依存させることで、クリップ変更時にリストを再取得
    property list<QtObject> rawEffectModels: {
        var _ = TimelineBridge ? TimelineBridge.clips : null // 依存関係トリガー
        return TimelineBridge && clipId >= 0 ? TimelineBridge.getClipEffectsModel(clipId) : []
    }
    // フィルタリング
    property var filterModels: {
        var list = [];
        for(var i=0; i<rawEffectModels.length; i++) {
            var eff = rawEffectModels[i];
            if (eff.id !== "rect" && eff.id !== "text" && eff.id !== "transform") {
                list.push(eff);
            }
        }
        return list;
    }

    // エフェクトモデルからパラメータを抽出して適用するロジック
    function updateParams() {
        for(let i=0; i<rawEffectModels.length; i++) {
            let eff = rawEffectModels[i];
            if (eff.id === "text" && eff.enabled) {
                // パラメータ適用
                if (eff.params["text"] !== undefined) root.textContent = eff.params["text"];
                if (eff.params["size"] !== undefined) root.textSize = eff.params["size"];
                if (eff.params["color"] !== undefined) root.color = eff.params["color"];
            }
        }
    }

    Connections {
        target: TimelineBridge
        function onSelectedClipDataChanged() { updateParams(); }
    }
    
    Component.onCompleted: updateParams()
    
    function getBlurPadding() {
        for(let i=0; i<rawEffectModels.length; i++) {
            if(rawEffectModels[i].id === "blur" && rawEffectModels[i].enabled) {
                return (rawEffectModels[i].params["size"] || 0);
            }
        }
        return 0;
    }
    property real blurPadding: getBlurPadding()

    // --- テクスチャ生成パイプライン ---
    
    resources: [
        // 1. ソースコンテンツ (生のテキスト)
        Item {
            id: sourceItem
            visible: true
            
            property int padding: root.blurPadding > 0 ? Math.ceil(root.blurPadding * 3) : 10
            width: textItem.implicitWidth + padding * 2
            height: textItem.implicitHeight + padding * 2

            Text {
                id: textItem
                anchors.centerIn: parent
                text: root.textContent
                font.pixelSize: root.textSize
                color: root.color
                style: Text.Outline; styleColor: "black"
                
                // 【最重要修正】 NativeRendering(CPU)を廃止しGPU描画へ変更
                renderType: Text.QtRendering 
                antialiasing: true
            }
        },

        EffectChain {
            id: effector
            sourceItem: sourceItem
            effectModels: root.filterModels
            width: sourceItem.width
            height: sourceItem.height
            visible: true 
        },

        // 2. テクスチャ化プロキシ
        // effectorをレンダリングし、結果を保持するが画面には表示しない
        ShaderEffectSource {
            id: textureSource
            sourceItem: effector
            hideSource: true // ソースを画面から隠す
            live: false      // 自動更新を切る (スレッドエラー回避)
            visible: true    // カリング回避のため visible: true (hideSourceがあるので画面には出ない)
            recursive: true  // 入れ子のエフェクトを許可
            FrameAnimation {
                running: true
                onTriggered: textureSource.scheduleUpdate()
            }
        }
    ]

    Model {
        source: "#Rectangle"
        scale: Qt.vector3d(sourceItem.width / 100, sourceItem.height / 100, 1)
        opacity: root.opacity
        materials: DefaultMaterial {
            diffuseMap: Texture { sourceItem: textureSource }
            lighting: DefaultMaterial.NoLighting
            blendMode: DefaultMaterial.SourceOver
        }
    }
}
