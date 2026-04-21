import QtQuick
import QtQuick.Effects

Item {
    id: base

    // ObjectRenderer から Binding で注入される
    property var params
    property Item source
    // ソースアイテムを非表示にしつつ、テクスチャとして利用可能にするプロキシ
    property alias sourceProxy: proxySource
    property QtObject effectModel
    property int frame: 0
    // QMLバインディング再評価用（params/keyframes変更を確実に検知）
    property int _rev: 0
    // 【自動描画範囲拡張】このエフェクトが必要とする追加余白をピクセル単位で宣言する。
    // 各エフェクト QML がこのプロパティをオーバーライドして自身の展開量を返す。
    // ObjectRenderer がこれを集計してキャンバスサイズを自動決定する。
    property var expansion: ({
        "top": 0,
        "right": 0,
        "bottom": 0,
        "left": 0
    })
    property var clipEvalParams: ({
    })
    property int _paramRev: 0

    // 【統一API】キーフレーム優先評価（ECS同期）
    function evalParam(key, fallback) {
        var _ = base._paramRev;
        if (base.clipEvalParams && base.effectModel && base.effectModel.id) {
            var effParams = base.clipEvalParams[base.effectModel.id];
            if (effParams !== undefined && effParams[key] !== undefined) {
                var v = effParams[key];
                if (v !== undefined && v !== null)
                    return v;

            }
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

    ShaderEffectSource {
        id: proxySource

        sourceItem: base.source
        hideSource: true
        visible: true
        opacity: 0
    }

    Connections {
        function onParamChanged(key, value) {
            base._rev++;
        }

        function onParamsChanged() {
            base._rev++;
        }

        function onKeyframeTracksChanged() {
            base._rev++;
        }

        target: base.effectModel
    }

}
