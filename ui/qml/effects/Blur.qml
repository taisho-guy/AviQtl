import QtQuick 2.15
import Qt5Compat.GraphicalEffects

Item {
    property var params: ({})
    property variant source

    FastBlur {
        anchors.fill: parent
        source: parent.source
        // 値の変更を強制的に検知させる
        // paramsオブジェクト自体が置換されない場合でもプロパティアクセスで値を取得
        // 修正: paramsプロパティへの依存を明示的に記述し、再評価をトリガー
        property var p: parent.params
        radius: (p && p.size !== undefined) ? p.size : 0
        transparentBorder: true
        cached: false
    }
}