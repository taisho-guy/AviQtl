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
        radius: (parent.params && typeof parent.params.size === 'number') ? parent.params.size : 0
        transparentBorder: true
        cached: false
    }
}