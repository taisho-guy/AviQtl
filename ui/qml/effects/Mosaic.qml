import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    property real size: Math.max(1, root.evalNumber("size", 10))

    ShaderEffectSource {
        anchors.fill: parent
        sourceItem: root.source
        // ソースのサイズをモザイクサイズで割ってテクスチャ解像度を落とす
        textureSize: Qt.size(Math.max(1, Math.ceil(width / root.size)), Math.max(1, Math.ceil(height / root.size)))
        smooth: false // 最近傍補間（ドット絵状）で拡大表示する
        visible: root.source !== null
    }

}
