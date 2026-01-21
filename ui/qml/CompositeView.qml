import QtQuick 2.15
import QtQuick3D 6.0
import QtQml 2.15
import "objects"

Item {
    id: root
    anchors.fill: parent

    // 背景（余白部分）は黒
    Rectangle {
        anchors.fill: parent
        color: "black"
        z: -2
    }

    // プレビューコンテナ（アスペクト比維持）
    View3D {
        id: view
        camera: mainCamera
        
        // プロジェクト設定の解像度を取得（未設定時はFHD）
        property int projW: TimelineBridge ? TimelineBridge.projectWidth : 1920
        property int projH: TimelineBridge ? TimelineBridge.projectHeight : 1080
        
        // アスペクト比計算
        property double aspect: projW / projH
        
        // 親に収まる最大サイズを計算 (Letterboxing)
        width: Math.min(parent.width, parent.height * aspect)
        height: Math.min(parent.height, parent.width / aspect)
        anchors.centerIn: parent

        focus: true
        Keys.onSpacePressed: {
            if (TimelineBridge) TimelineBridge.togglePlay()
        }

        // プロジェクト領域の背景
        Rectangle {
            anchors.fill: parent
            color: "#0a0a0a"
            z: -1
        }

        // カメラ設定 (必須)
        PerspectiveCamera {
            id: mainCamera
            // 1080p を視野に収める距離: 540 / tan(30deg) ≈ 935.3
            // クリップ範囲も調整
            position: Qt.vector3d(0, 0, 936)
            clipFar: 5000
        }
        DirectionalLight {
            eulerRotation.x: -30
            z: 1000
        }
        
        environment: SceneEnvironment {
            id: sceneEnv
            backgroundMode: SceneEnvironment.Color
            clearColor: "#000000"
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
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
                visible: false // 邪魔なので一旦隠す
            }
        }

        // Instantiator の親となるノード
        Node {
            id: sceneRoot
        }

        // 動的オブジェクト生成用
        Instantiator {
            model: TimelineBridge ? TimelineBridge.activeClips : []
            
            onObjectAdded: (index, object) => {
                console.log("[CompositeView] Adding object to scene:", index)
                object.parent = sceneRoot
            }
            onObjectRemoved: (index, object) => {
                console.log("[CompositeView] Removing object from scene:", index)
                object.parent = null
            }

            delegate: Node {
                // 座標変換: 中心(0,0)、Y軸下プラス(AviUtl互換)
                // Qt3DはY上がプラスなので、入力を反転させる
                x: (modelData.x !== undefined) ? modelData.x : 0
                y: (modelData.y !== undefined) ? -modelData.y : 0
                z: modelData.layer * 50 // レイヤー間隔を広げる
                
                // 回転 (Z軸)
                eulerRotation.z: (modelData.rotation !== undefined) ? -modelData.rotation : 0
                // 不透明度 (全体)
                opacity: (modelData.opacity !== undefined) ? modelData.opacity : 1.0

                RectObject {
                    visible: modelData.type === "rect"
                    sizeW: (modelData.width !== undefined) ? modelData.width : 100
                    sizeH: (modelData.height !== undefined) ? modelData.height : 100
                    color: (modelData.color !== undefined) ? modelData.color : "#ffffff"
                    opacity: (modelData.opacity !== undefined) ? modelData.opacity : 1.0
                }

                TextObject {
                    visible: modelData.type === "text"
                    textContent: (modelData.text !== undefined) ? modelData.text : ""
                    textSize: (modelData.textSize !== undefined) ? modelData.textSize : 64
                    color: (modelData.color !== undefined) ? modelData.color : "#ffffff"
                    opacity: (modelData.opacity !== undefined) ? modelData.opacity : 1.0
                }
            }
        }
    }
}