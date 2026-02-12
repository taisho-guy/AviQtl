import QtQml 2.15
import QtQuick 2.15
import QtQuick.Effects
import QtQuick3D 6.0
import "common" as Common
import "common/Logger.js" as Logger

Item {
    id: root

    // レイヤーの表示状態を取得する関数（外部から注入される）
    property var getLayerVisible: function(layer) {
        return true;
    }
    readonly property int hiddenZ: -9999
    // Component cache to prevent redundant Qt.createComponent calls
    property var _componentCache: ({
    })

    function getCachedComponent(url) {
        if (_componentCache[url])
            return _componentCache[url];

        var c = Qt.createComponent(url, Component.Asynchronous);
        _componentCache[url] = c;
        return c;
    }

    anchors.fill: parent

    // 2Dレンダー（sourceItem/effects/ShaderEffectSource）を必ずQQuickWindow配下に置くためのホスト
    // visible:false でもWindow配下に居ればSceneGraph/Timerが正常に動く
    // 重要:
    // visible:false は SceneGraph から外れる → ShaderEffectSource の更新が止まりやすい。
    // 表示を消したいだけなら visible:true のまま opacity:0 に寄せる。
    Item {
        id: offscreenRenderHost

        anchors.fill: parent
        visible: true
        opacity: 0
        enabled: false
        z: hiddenZ
    }

    // 動的レンダーパス生成
    Item {
        id: compositionRoot
        anchors.fill: parent
        visible: false
        
        property var renderPasses: []
        
        Repeater {
            model: compositionRoot.renderPasses
            
            delegate: Item {
                id: passContainer
                anchors.fill: parent
                
                ShaderEffectSource {
                    id: passOutput
                    sourceItem: passContainer
                    
                    // 【重要】
                    // visible: false にすると、Qtの最適化で更新が止まることがある。
                    // 確実に更新させるには live: true に加え、
                    // 「画面外に配置」するか「透明度0で表示」するハックが必要な場合がある。
                    // ここでは一旦 hideSource: false でテストすることを推奨。
                    
                    live: true
                    hideSource: false // デバッグ: これをfalseにして、重複して表示されるか確認
                    visible: false    // デバッグ: これをtrueにすると、画面左上に小さく出るはず
                    
                    // サイズ明示（必須）
                    width: compositionRoot.width
                    height: compositionRoot.height
                    
                    // テクスチャ生成のトリガー
                    recursive: false
                }
            }
        }
    }

    // 背景（余白部分）は黒
    Rectangle {
        anchors.fill: parent
        color: "black"
        z: -2
    }

    // プレビューコンテナ（アスペクト比維持）
    View3D {
        id: view

        // プロジェクト設定の解像度を取得（未設定時はFHD）
        property int projW: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.width : 1920
        property int projH: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.height : 1080
        // アスペクト比計算
        property double aspect: projW / projH
        // 現在のクリップ内での相対時間 (0.0 ~ 1.0)
        // 他のコンポーネントから参照される可能性を考慮して定義
        property double currentClipTimeRatio: (TimelineBridge && TimelineBridge.transport) ? Math.max(0, Math.min(1, (TimelineBridge.transport.currentFrame - TimelineBridge.clipStartFrame) / TimelineBridge.clipDurationFrames)) : 0

        camera: mainCamera
        // 親に収まる最大サイズを計算 (Letterboxing)
        width: Math.min(parent.width, parent.height * aspect)
        height: Math.min(parent.height, parent.width / aspect)
        anchors.centerIn: parent
        focus: true
        Keys.onSpacePressed: {
            if (TimelineBridge && TimelineBridge.transport)
                TimelineBridge.transport.togglePlay();

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

            property real distance: view.projH / (2 * Math.tan(fieldOfView * Math.PI / 360))

            // 動的カメラ距離計算: projH/2 / tan(fieldOfView/2)
            fieldOfView: 30
            position: Qt.vector3d(0, 0, distance)
            clipFar: 5000
        }

        DirectionalLight {
            eulerRotation.x: -30
            z: 1000
        }

        // プロジェクトの枠線（解像度ガイド）
        Node {
            // 画面中央を (0,0) としたときの枠
            Model {
                source: "#Rectangle"
                scale: Qt.vector3d((TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.width / 100 : 19.2, (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.height / 100 : 10.8, 1)
                position: Qt.vector3d(0, 0, -10)
                visible: false // 邪魔なので一旦隠す

                materials: DefaultMaterial {
                    // 枠線だけ表示したいが、簡易的に半透明の黒背景として表示
                    diffuseColor: "#22000000"
                }
                // 奥に配置

            }

        }

        // Instantiator の親となるノード
        Node {
            id: sceneRoot
        }

        // 動的オブジェクト生成用
        Instantiator {
            // 2D要素のレンダリングホスト参照を渡す
            property Item renderHost: offscreenRenderHost

            // QVariantListではなく、C++のAbstractListModelを使用
            model: TimelineBridge ? TimelineBridge.clipModel : null
            onObjectAdded: (index, object) => {
                object.parent = sceneRoot;
                // renderHostを注入（BaseObjectが期待している場合）
                if (object.hasOwnProperty("renderHost"))
                    object.renderHost = offscreenRenderHost;

            }
            onObjectRemoved: (index, object) => {
                object.parent = null;
            }

            delegate: Node {
                id: clipNode

                // モデルロールから直接値を取得
                // パラメータを一度だけ取得してキャッシュ (undefinedチェックのオーバーヘッド削減)
                readonly property var p: model.params || {
                }
                readonly property real px: p.x || 0
                readonly property real py: p.y || 0
                readonly property real pz: p.z || 0
                readonly property real pRotX: p.rotationX || 0
                readonly property real pRotY: p.rotationY || 0
                readonly property real pRotZ: p.rotationZ || 0
                readonly property real pScale: p.scale || 100
                readonly property real pAspect: p.aspect || 0
                readonly property real pOpacity: p.opacity || 1
                // 拡大率と縦横比
                readonly property real baseScale: pScale * 0.01
                readonly property real aspectX: pAspect >= 0 ? (1 + pAspect) : 1
                readonly property real aspectY: pAspect < 0 ? (1 - pAspect) : 1

                function dbg(msg) {
                    Logger.log("[CompositeView][clipId=" + model.id + "][type=" + model.type + "] " + msg, TimelineBridge);
                }

                // レイヤーが非表示の場合は描画しない
                visible: {
                    return root.getLayerVisible(model.layer);
                }
                // 座標変換: 中心(0,0)、Y軸下プラス(AviUtl互換)
                // Qt3DはY上がプラスなので、入力を反転させる
                x: px
                y: -py
                z: pz + (model.layer * 5)
                // 中心座標 (Pivot)
                pivot: Qt.vector3d(p.anchorX || 0, -(p.anchorY || 0), p.anchorZ || 0)
                // 3軸回転
                eulerRotation.x: pRotX
                eulerRotation.y: -pRotY
                eulerRotation.z: -pRotZ
                scale.x: baseScale * aspectX
                scale.y: baseScale * aspectY
                scale.z: baseScale
                // 不透明度 (全体)
                opacity: pOpacity

                // Loader (2D) は 3D シーン内では機能しないため、
                // Qt.createComponent を使用して Node 派生クラスを動的に生成する
                Common.NodeLoader {
                    id: objectContainer

                    source: model.qmlSource || ""
                    properties: {
                        "opacity": clipNode.pOpacity,
                        "clipId": model.id,
                        "clipStartFrame": model.startFrame,
                        "clipDurationFrames": model.durationFrames,
                        "renderHost": offscreenRenderHost,
                        "clipParams": Qt.binding(function() {
                            return model.params || {
                            };
                        })
                    }
                    componentFactory: root.getCachedComponent
                }

            }

        }

        environment: SceneEnvironment {
            id: sceneEnv

            backgroundMode: SceneEnvironment.Color
            clearColor: "#000000"
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

    }

}
