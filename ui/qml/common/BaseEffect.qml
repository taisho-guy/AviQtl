import QtQuick

Item {
    id: base

    // ObjectRenderer から Binding で注入される
    property Item source: null
    property var params
    property QtObject effectModel
    property int frame: 0

    // QMLバインディング再評価用（params/keyframes変更を確実に検知）
    property int _rev: 0
    Connections {
        target: base.effectModel
        function onParamChanged(key, value) { base._rev++ }
        function onParamsChanged() { base._rev++ }
        function onKeyframeTracksChanged() { base._rev++ }
    }

    // 【統一API】キーフレーム優先評価
    function evalParam(key, fallback) {
        // これを参照することで、_rev の変化＝params変更で依存が更新される
        var _ = base._rev
        if (base.effectModel && base.effectModel.evaluatedParam) {
            var v = base.effectModel.evaluatedParam(key, base.frame);
            if (v !== undefined && v !== null)
                return v;

        }
        if (base.params && base.params[key] !== undefined)
            return base.params[key];

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

    // implicitSize を使用（子要素が自動的に参照）
    implicitWidth: source ? source.width : 1
    implicitHeight: source ? source.height : 1
    // 明示的サイズも設定
    width: implicitWidth > 0 ? implicitWidth : 1
    height: implicitHeight > 0 ? implicitHeight : 1
}
