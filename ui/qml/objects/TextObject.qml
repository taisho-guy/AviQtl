import QtQuick 2.15
import QtQuick3D 6.0

Node {
    id: root
    
    // 外部から制御されるプロパティ
    property string textContent: "Text"
    property int textSize: 64

    // 隠しテクスチャソース (インスタンスごとに独立させる必要がある)
    // ※パフォーマンス最適化のため、実際には共有リソースマネージャを使うべきだが、まずは独立させる
    Item {
        id: textureSource
        width: 512; height: 512
        visible: false
        layer.enabled: true
        Text {
            anchors.centerIn: parent
            text: root.textContent
            font.pixelSize: root.textSize
            color: "white"
            style: Text.Outline; styleColor: "black"
        }
    }

    Model {
        source: "#Rectangle"
        scale: Qt.vector3d(4, 4, 1)
        materials: DefaultMaterial {
            diffuseMap: Texture { sourceItem: textureSource }
            blendMode: DefaultMaterial.SourceOver
            lighting: DefaultMaterial.NoLighting
        }
    }
}
