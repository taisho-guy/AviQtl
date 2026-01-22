import QtQuick
import QtQuick.Effects

Item {
    id: root
    property var effectModels: [] // C++から渡される EffectModel のリスト
    property Item sourceItem

    width: root.sourceItem ? root.sourceItem.width : 0
    height: root.sourceItem ? root.sourceItem.height : 0

    MultiEffect {
        id: effector
        source: root.sourceItem
        anchors.fill: parent
        
        // 動的にパラメータを探してバインドするヘルパー関数
        function getParam(effectId, paramName, defaultVal) {
            if (!root.effectModels) return defaultVal;
            for (var i = 0; i < root.effectModels.length; i++) {
                if (root.effectModels[i].id === effectId && root.effectModels[i].enabled) {
                    var val = root.effectModels[i].params[paramName];
                    return (val !== undefined) ? val : defaultVal;
                }
            }
            return defaultVal;
        }

        // --- Blur (ぼかし) ---
        property real blurSize: getParam("blur", "size", 0)
        blurEnabled: blurSize > 0
        blur: blurSize / 64.0
        blurMax: 64

        // --- Color Correction (色調補正) ---
        // Rina: 100基準 -> MultiEffect: -1.0 ~ 1.0
        brightness: (getParam("color_correction", "brightness", 100) - 100) / 100.0
        contrast: (getParam("color_correction", "contrast", 100) - 100) / 100.0
        saturation: (getParam("color_correction", "saturation", 100) - 100) / 100.0
        
        // --- Glow (発光) ---
        // MultiEffectのShadow機能で代用
        property real glowStrength: getParam("glow", "strength", 0)
        shadowEnabled: glowStrength > 0
        shadowBlur: getParam("glow", "radius", 0) / 64.0
        shadowColor: getParam("glow", "color", "#ffffff")
        shadowOpacity: glowStrength / 100.0
    }

    // 外部エフェクトパイプライン
    Repeater {
        model: root.effectModels
        delegate: Loader {
            // qmlSourceがある場合のみロード
            active: modelData.qmlSource !== "" && modelData.enabled
            source: modelData.qmlSource
            
            onLoaded: {
                if (item) {
                    // パラメータの注入
                    item.params = modelData.params
                    
                    // 入力ソースの連鎖
                    // 簡易実装: 最初の外部エフェクトはMultiEffectの結果を受け取る
                    // 2つ目以降も現状はMultiEffectの結果を受け取る（並列適用）
                    // ※直列化にはInstantiatorによる高度なチェーン管理が必要
                    item.source = effector
                    
                    item.width = Qt.binding(() => root.width)
                    item.height = Qt.binding(() => root.height)
                }
            }
        }
    }
}