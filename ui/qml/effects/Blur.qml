import QtQuick
import QtQuick.Effects

Item {
    id: root
    // パイプライン入力: 前のステージの映像を受け取る
    property Item source
    
    // データモデルからのパラメータ
    property var params

    // フィルタの実装 (MultiEffectを使う場合でも、ここは1つのフィルタとして振る舞う)
    MultiEffect {
        anchors.fill: parent
        source: root.source
        
        blurEnabled: true
        blurMax: 64
        // パラメータ解釈の責任はこのファイルが持つ
        blur: (root.params && root.params["size"] !== undefined) ? (Number(root.params["size"]) / 64.0) : 0.0
    }
}