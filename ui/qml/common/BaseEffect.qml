import QtQuick
import QtQuick.Effects

Item {
    id: base

    // ObjectRenderer / BaseObject から Qt.binding で注入される ECS 評価済みキャッシュ
    // { effectId: { paramName: value, ... }, ... } の形式
    property var ecsCache: ({
    })
    property Item source
    property alias sourceProxy: proxySource
    property QtObject effectModel: null
    property int frame: 0
    // 各エフェクトが必要とする追加余白をピクセル単位で宣言する
    property var expansion: ({
        "top": 0,
        "right": 0,
        "bottom": 0,
        "left": 0
    })

    // 【統一API】ECS キャッシュから直接取得。カウンター依存なし。
    // ecsCache[effectModel.id][key] への参照が QML エンジンの宣言的依存グラフに
    // 自動登録されるため、ecsCache が差し替わると呼び出し元が自動再評価される。
    function evalParam(key, fallback) {
        if (base.ecsCache && base.effectModel && base.effectModel.id) {
            var m = base.ecsCache[base.effectModel.id];
            if (m !== undefined && m[key] !== undefined && m[key] !== null)
                return m[key];

        }
        return fallback;
    }

    function evalNumber(key, fallback) {
        var v = evalParam(key, fallback);
        return (v !== undefined && v !== null && v !== "") ? Number(v) : fallback;
    }

    function evalColor(key, fallback) {
        var v = evalParam(key, fallback);
        return (typeof v === "string") ? v : fallback;
    }

    ShaderEffectSource {
        id: proxySource

        sourceItem: base.source
        hideSource: true
        visible: true
        opacity: 0
    }

}
