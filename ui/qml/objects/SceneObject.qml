import "../common" as Common
import QtQuick 2.15
import QtQuick.Shapes 1.15
import Rina 1.0

Common.BaseObject {
    // 将来的にはここで「子シーンをオフスクリーン描画」したテクスチャを貼る
    // MVPとしては境界可視化のみ

    id: root

    // TimelineBridge から現在のフレームやシーン情報を取得する前提
    property int targetSceneId: evalParam("scene", "targetSceneId", 0)
    property real speed: evalParam("scene", "speed", 1.0)
    property int offset: evalParam("scene", "offset", 0)
    property real opacity: evalParam("scene", "opacity", 1.0)

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: "#66ffffff"
        border.width: 1
        radius: 4
        opacity: root.opacity
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.top: parent.top
        anchors.topMargin: 2
        text: "Scene #" + root.targetSceneId + (root.targetSceneId === 0 ? " (Self?)" : "")
        color: "white"
        font.pixelSize: 10
        opacity: root.opacity
    }

}
