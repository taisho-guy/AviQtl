import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: root

    // JSONで定義したパラメータを取得 (第1引数のeffectIdはBaseObject内では無視されますが、慣習として指定)
    property string sourcePath: String(evalParam("audio", "source", ""))
    property real startOffset: Number(evalParam("audio", "startOffset", 0))
    property real speed: Number(evalParam("audio", "speed", 100))
    property real volume: Number(evalParam("audio", "volume", 1))
    property real pan: Number(evalParam("audio", "pan", 0))
    property bool mute: evalParam("audio", "mute", false)

    // 音声オブジェクトは画面に描画されないため、ダミーの不可視Itemを指定
    sourceItem: Item {
        width: 1
        height: 1
        visible: false
    }

}
