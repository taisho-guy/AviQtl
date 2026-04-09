import QtQuick

BaseEffect {
    id: root

    property alias computeShader: compEffect.computeShader
    property alias autoWorkGroup: compEffect.autoWorkGroup
    property alias storageBuffers: compEffect.storageBuffers
    property alias workGroupSizeX: compEffect.workGroupSizeX
    property alias workGroupSizeY: compEffect.workGroupSizeY
    property alias computeError: compEffect.error
    // JSONのパラメータ名とシェーダーのUniform名が異なる場合のマッピング
    // 例: { "mix": "mixAmount" }
    property var uniformMapping: ({
    })

    signal ssboReadbackCompleted(int binding, var data)

    function requestReadback(binding) {
        compEffect.requestReadback(binding);
    }

    // params からキーフレーム評価済みの Uniform オブジェクトを自動構築する
    function buildUniforms() {
        var result = {
        };
        // _rev を参照することで、キーフレームの現在値が変化した際に再評価される
        var _ = root._rev;
        if (!root.params)
            return result;

        var keys = Object.keys(root.params);
        for (var i = 0; i < keys.length; i++) {
            var key = keys[i];
            var val = root.evalParam(key, root.params[key]);
            // bool型はシェーダー用に1/0に変換
            if (typeof val === "boolean")
                val = val ? 1 : 0;

            var uniformName = root.uniformMapping[key] || key;
            result[uniformName] = val;
        }
        return result;
    }

    ComputeEffect {
        id: compEffect

        anchors.fill: parent
        source: root.sourceProxy
        uniforms: root.buildUniforms()
        autoWorkGroup: true
        onSsboReadbackCompleted: (binding, data) => {
            root.ssboReadbackCompleted(binding, data);
        }
    }

}
