import QtQuick 2.15
import QtQuick3D 6.0

View3D {
    id: view
    anchors.fill: parent

    focus: true
    Keys.onSpacePressed: {
        if (TimelineBridge) TimelineBridge.togglePlay()
    }

    // AviUtlライクな平行投影カメラ
    OrthographicCamera {
        id: camera
        z: 1000
    }
    
    environment: SceneEnvironment {
        clearColor: "#101010" // 少し明るい黒（AviUtlっぽい背景色）
        backgroundMode: SceneEnvironment.Color
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
    }

    // 照明 (2D的な見た目にするためアンビエント強め)
    DirectionalLight {
        eulerRotation.x: -30
        brightness: 1.0
        ambientColor: "#808080"
    }

    // 2Dのテキストを描画する隠しレイヤー (Texture Source)
    Item {
        id: textSource
        width: 512; height: 512
        visible: false // 画面には直接出さない
        
        Text {
            anchors.centerIn: parent
            text: TimelineBridge ? TimelineBridge.textString : "Text"
            font.pixelSize: TimelineBridge ? TimelineBridge.textSize : 32
            color: "white"
            style: Text.Outline
            styleColor: "black"
            font.bold: true
        }
    }

    // テストオブジェクト: 動画レイヤーを模した板ポリゴン
    Node {
        id: layer1
        // ここが重要！ TimelineBridgeの値とバインドする
        // C++のプロパティが変わると自動的に画面上の位置も変わる
        position: Qt.vector3d(TimelineBridge ? TimelineBridge.objectX : 0, 0, 0)
        visible: TimelineBridge ? TimelineBridge.isClipActive : false

        Model {
            source: "#Rectangle"
            scale: Qt.vector3d(3, 2, 1) // 300x200px 相当
            materials: DefaultMaterial {
                diffuseColor: "#66aa99" // AviUtlのレイヤーっぽい色
            }
        }
    }

    // テキストオブジェクトレイヤー
    Node {
        id: textLayer
        position: Qt.vector3d(TimelineBridge ? TimelineBridge.objectX : 0, TimelineBridge ? TimelineBridge.objectY : 0, 10)
        visible: TimelineBridge ? TimelineBridge.isClipActive : false

        Model {
            source: "#Rectangle"
            scale: Qt.vector3d(4, 4, 1)
            materials: DefaultMaterial {
                diffuseMap: Texture { sourceItem: textSource }
                blendMode: DefaultMaterial.SourceOver
                lighting: DefaultMaterial.NoLighting
            }
        }
    }
}