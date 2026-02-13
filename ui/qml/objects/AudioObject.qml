import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: root

    // ObjectRenderer からのプロパティ要求に対応（ダミー定義）
    property string source: ""
    property var params: ({})
    property var effectModel: null
    property int frame: 0

    // JSONで定義したパラメータを取得 (第1引数のeffectIdはBaseObject内では無視されますが、慣習として指定)
    property string sourcePath: String(evalParam("audio", "source", ""))
    property real volume: Number(evalParam("audio", "volume", 1.0))
    property real pan: Number(evalParam("audio", "pan", 0.0))
    property bool mute: evalParam("audio", "mute", false)

    // 音声オブジェクトは画面に描画されないため、ダミーの不可視Itemを指定
    sourceItem: Item {
        width: 1; height: 1
        visible: false
    }
}