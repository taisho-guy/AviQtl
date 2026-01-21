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

    // プロジェクトの枠線（解像度ガイド）
    Node {
        // 画面中央を (0,0) としたときの枠
        Model {
            source: "#Rectangle"
            scale: Qt.vector3d(
                TimelineBridge ? TimelineBridge.projectWidth / 100 : 19.2, 
                TimelineBridge ? TimelineBridge.projectHeight / 100 : 10.8, 
                1
            )
            materials: DefaultMaterial {
                // 枠線だけ表示したいが、簡易的に半透明の黒背景として表示
                diffuseColor: "#22000000" 
            }
            // 奥に配置
            position: Qt.vector3d(0, 0, -10)
        }
    }

    // 動的オブジェクトローダー
    Loader3D {
        id: objectLoader
        visible: TimelineBridge ? TimelineBridge.isClipActive : false

        // TimelineBridgeの状態に応じて読み込むファイルを切り替える
        source: {
            if (!TimelineBridge) return ""
            switch (TimelineBridge.activeObjectType) {
                case "text": return "qrc:/qml/objects/TextObject.qml"
                case "rect": return "qrc:/qml/objects/RectObject.qml"
                default: return ""
            }
        }

        // ロードされたアイテムに対してプロパティをバインドする
        onLoaded: {
            if (item) {
                // 共通プロパティのバインディング
                item.positionValue = Qt.binding(function() {
                    return Qt.vector3d(TimelineBridge.objectX, TimelineBridge.objectY, 10)
                })
                
                // テキスト固有プロパティのバインディング (存在する場合のみ)
                if (TimelineBridge.activeObjectType === "text") {
                    item.textContent = Qt.binding(function() { return TimelineBridge.textString })
                    item.textSize = Qt.binding(function() { return TimelineBridge.textSize })
                }
            }
        }
    }
}