import QtQuick
import QtQuick3D
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: base

    // ObjectRendererが期待するプロパティをダミー定義（警告回避）
    property var source: undefined
    property var params: ({})
    property var effectModel: null
    property int frame: 0
    property int width: containerItem.width
    property int height: containerItem.height

    // ImageObjectやRectObjectと同じパターンに従う
    sourceItem: Item {
        id: containerItem
        // プロジェクトサイズをデフォルトとする
        width: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.width : 1920
        height: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.height : 1080
        visible: false // ObjectRendererが処理するため直接表示しない

        Image {
            id: img
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            cache: false

            // clipIdを使用してフレームを取得
            source: "image://videoFrame/" + (base.clipId ? base.clipId : "debug") + "?v=" + base.relFrame
        }
    }

    // 3DモデルとしてレンダリングされたOutputを表示
    Model {
        source: "#Rectangle" // プリミティブ平面
        scale: Qt.vector3d(base.sourceItem.width / 100, base.sourceItem.height / 100, 1)

        materials: DefaultMaterial {
            lighting: DefaultMaterial.NoLighting
            blendMode: DefaultMaterial.SourceOver
            diffuseMap: Texture {
                sourceItem: renderer.output
            }
        }
    }
}
