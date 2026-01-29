import QtQuick
import QtQuick.Effects

Item {
    id: root
    // パイプライン入力: 前のステージの映像を受け取る
    property Item source
    
    // データモデルからのパラメータ
    property var params
    property QtObject effectModel // 親から渡される生モデル
    property int frame: 0

    // フィルタの実装 (MultiEffectを使う場合でも、ここは1つのフィルタとして振る舞う)
    MultiEffect {
        anchors.fill: parent
        source: root.source
        
        blurEnabled: true
        blurMax: 64
        
        // effectModelが来ていればそちらを優先（常に最新）、なければparamsを使う
        readonly property var activeParams: root.effectModel ? root.effectModel.params : root.params
        
        readonly property var sizeValue: {
            if (root.effectModel && root.effectModel.evaluatedParam) {
                var v = root.effectModel.evaluatedParam("size", root.frame);
                if (v !== undefined && v !== null) return v;
            }
            return activeParams ? activeParams["size"] : 0;
        }

        // パラメータ解釈の責任はこのファイルが持つ
        blur: (sizeValue !== undefined && sizeValue !== null) ? (Number(sizeValue) / 64.0) : 0.0
    }
}