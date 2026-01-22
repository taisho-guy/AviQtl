import QtQuick
import QtQuick.Effects

Item {
    id: root
    property var effectModels: [] // C++から渡される EffectModel のリスト
    property Item sourceItem

    MultiEffect {
        id: effector
        source: root.sourceItem
        // 外部のItemにはアンカーできないため、サイズをバインドする
        width: root.sourceItem ? root.sourceItem.width : 0
        height: root.sourceItem ? root.sourceItem.height : 0
        anchors.centerIn: parent
        
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
}