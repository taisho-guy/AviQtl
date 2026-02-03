import QtQuick
import QtQuick3D
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: base

    property string imagePath: String(evalParam("image", "path", ""))
    property int fillMode: Number(evalParam("image", "fillMode", Image.PreserveAspectFit))
    property bool smooth: evalParam("image", "smooth", true)
    property real imageOpacity: Number(evalParam("image", "opacity", 1))

    // 3Dシーンへのマッピング定義
    Model {
        source: "#Rectangle"
        scale: Qt.vector3d((renderer.output.sourceItem ? renderer.output.sourceItem.width : base.sourceItem.width) / 100, (renderer.output.sourceItem ? renderer.output.sourceItem.height : base.sourceItem.height) / 100, 1)
        opacity: base.imageOpacity

        materials: DefaultMaterial {
            lighting: DefaultMaterial.NoLighting
            blendMode: DefaultMaterial.SourceOver

            diffuseMap: Texture {
                sourceItem: renderer.output
            }

        }

    }

    sourceItem: Image {
        id: imageSource

        source: base.imagePath
        fillMode: base.fillMode
        smooth: base.smooth
        opacity: base.imageOpacity
        // プロジェクト解像度に合わせて表示
        width: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.width : 1920
        height: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.height : 1080
        onStatusChanged: {
            if (status === Image.Error)
                console.error("[ImageObject] 画像読み込みエラー:", source);

        }
        visible: false
        // パディング対応（ぼかしエフェクト等で端が切れないようにする）
        anchors.margins: -base.padding
    }

}
