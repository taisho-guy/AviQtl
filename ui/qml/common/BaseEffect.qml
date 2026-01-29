import QtQuick

Item {
    id: base
    
    // ObjectRenderer から Binding で注入される
    property Item source: null
    property var params
    property QtObject effectModel
    property int frame: 0
    
    // 明示的にサイズを設定（子の MultiEffect が参照）
    width: source ? source.width : 0
    height: source ? source.height : 0
    
    // 【統一API】キーフレーム優先評価
    function evalParam(key, fallback) {
        if (base.effectModel && base.effectModel.evaluatedParam) {
            var v = base.effectModel.evaluatedParam(key, base.frame);
            if (v !== undefined && v !== null) return v;
        }
        if (base.params && base.params[key] !== undefined) {
            return base.params[key];
        }
        return fallback;
    }
    
    // 数値専用（型変換込み）
    function evalNumber(key, fallback) {
        return Number(evalParam(key, fallback));
    }
    
    // 色専用
    function evalColor(key, fallback) {
        var v = evalParam(key, fallback);
        return (typeof v === 'string') ? v : fallback;
    }
}