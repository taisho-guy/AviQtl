import QtQuick
import QtQuick.Effects

Item {
    id: root
    property var params: ({})
    property Item source
    property QtObject effectModel
    property int frame: 0

    // ヘルパー：キーフレーム優先評価
    function evalParam(key, fallback) {
        if (root.effectModel && root.effectModel.evaluatedParam) {
            var v = root.effectModel.evaluatedParam(key, root.frame);
            if (v !== undefined && v !== null) return Number(v);
        }
        if (root.params && root.params[key] !== undefined) {
            return Number(root.params[key]);
        }
        return fallback;
    }

    MultiEffect {
        anchors.fill: parent
        source: root.source
        
        readonly property real p_bright: root.evalParam("brightness", 100)
        readonly property real p_sat: root.evalParam("saturation", 100)
        readonly property real p_cont: root.evalParam("contrast", 100)

        brightness: (p_bright - 100) / 100.0
        saturation: (p_sat - 100) / 100.0
        contrast: (p_cont - 100) / 100.0
    }
}