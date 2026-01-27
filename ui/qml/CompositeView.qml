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
        
        // 現在のクリップ内での相対時間 (0.0 ~ 1.0)
        // 他のコンポーネントから参照される可能性を考慮して定義
        property double currentClipTimeRatio: TimelineBridge ? 
            Math.max(0.0, Math.min(1.0, (TimelineBridge.currentFrame - TimelineBridge.clipStartFrame) / TimelineBridge.clipDurationFrames)) : 0.0

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
            // QVariantListではなく、C++のAbstractListModelを使用
            model: TimelineBridge ? TimelineBridge.clipModel : null
            
            onObjectAdded: (index, object) => {
                object.parent = sceneRoot
            }
            onObjectRemoved: (index, object) => {
                object.parent = null
            }

            delegate: Node {
                // モデルロールから直接値を取得
                property var p: model.params 

                // 座標変換: 中心(0,0)、Y軸下プラス(AviUtl互換)
                // Qt3DはY上がプラスなので、入力を反転させる
                x: (p.x !== undefined) ? p.x : 0
                y: (p.y !== undefined) ? -p.y : 0
                z: ((p.z !== undefined) ? p.z : 0) + (model.layer * 5)
                
                // 中心座標 (Pivot)
                pivot: Qt.vector3d(
                    (p.anchorX !== undefined) ? p.anchorX : 0,
                    (p.anchorY !== undefined) ? -p.anchorY : 0,
                    (p.anchorZ !== undefined) ? p.anchorZ : 0
                )

                // 3軸回転
                eulerRotation.x: (p.rotationX !== undefined) ? p.rotationX : 0
                eulerRotation.y: (p.rotationY !== undefined) ? -p.rotationY : 0
                eulerRotation.z: (p.rotationZ !== undefined) ? -p.rotationZ : 0

                // 拡大率と縦横比
                property real baseScale: (p.scale !== undefined) ? p.scale / 100.0 : 1.0
                property real asp: (p.aspect !== undefined) ? p.aspect : 0.0
                scale: Qt.vector3d(
                    baseScale * (asp >= 0 ? (1.0 + asp) : 1.0),
                    baseScale * (asp < 0 ? (1.0 - asp) : 1.0),
                    baseScale
                )

                // 不透明度 (全体)
                opacity: (p.opacity !== undefined) ? p.opacity : 1.0

                RectObject {
                    visible: model.type === "rect"
                    sizeW: (p.sizeW !== undefined) ? p.sizeW : 100
                    sizeH: (p.sizeH !== undefined) ? p.sizeH : 100
                    color: (p.color !== undefined) ? p.color : "#ffffff"
                    opacity: (p.opacity !== undefined) ? p.opacity : 1.0
                    // 追加: IDを渡す
                    clipId: model.id
                }

                TextObject {
                    visible: model.type === "text"
                    textContent: (p.text !== undefined) ? p.text : ""
                    // 追加: IDを渡す
                    clipId: model.id
                }
            }
        }
    }
}