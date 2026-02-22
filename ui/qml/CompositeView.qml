import QtQml 2.15
import QtQuick 2.15
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
    // ─── グループ制御管理 ─────────────────────────────────────────
    property var groupControls: []

    function getCachedComponent(url) {
        if (_componentCache[url])
            return _componentCache[url];

        var c = Qt.createComponent(url, Component.Asynchronous);
        _componentCache[url] = c;
        return c;
    }

    function registerGroupControl(gc) {
        for (var i = 0; i < groupControls.length; ++i) {
            if (groupControls[i] === gc)
                return ;

        }
        groupControls.push(gc);
        groupControlsChanged(); // 配列変更を通知して再計算を促す
    }

    function unregisterGroupControl(gc) {
        var idx = -1;
        for (var i = 0; i < groupControls.length; ++i) {
            if (groupControls[i] === gc) {
                idx = i;
                break;
            }
        }
        if (idx !== -1) {
            groupControls.splice(idx, 1);
            groupControlsChanged();
        }
    }

    // 2Dレンダー（sourceItem/effects/ShaderEffectSource）を必ずQQuickWindow配下に置くためのホスト
    // visible:false でもWindow配下に居ればSceneGraph/Timerが正常に動く
    Item {
        id: offscreenRenderHost

        anchors.fill: parent
        visible: true
        opacity: 0
        enabled: false
        z: hiddenZ
    }

    // 背景
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        z: -2
    }

    // プレビューコンテナ
    View3D {
        id: view

        // プロジェクト設定の解像度を取得
        property int projW: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.width : 1920
        property int projH: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.height : 1080
        // アスペクト比計算
        property double aspect: projW / projH
        // 現在のクリップ内での相対時間 (0.0 ~ 1.0)
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

        // カメラ設定
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
            // 拡大率は親の影響を受けず自身のサイズとして適用（AviUtl仕様依存）

            model: TimelineBridge ? TimelineBridge.clipModel : null
            onObjectAdded: (index, object) => {
                object.parent = sceneRoot;
            }
            onObjectRemoved: (index, object) => {
                object.parent = null;
            }

            delegate: Node {
                id: clipNode

                // FB 収集用: BaseObject が参照できるよう layer を公開
                property int clipLayerRole: model.layer
                property Item fbRendererOutput: null // NodeLoader 完了後に接続
                // モデルロールから直接値を取得
                // パラメータを一度だけ取得してキャッシュ
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
                // ─── 実効トランスフォーム計算 (グループ制御適用) ───────────
                property var effectiveTransform: {
                    // 依存関係を明示的に登録 (groupControlsの変更を検知)
                    var _gcList = root.groupControls;
                    // 1. 適用可能なグループ制御を抽出
                    var activeGroups = [];
                    for (var i = 0; i < root.groupControls.length; ++i) {
                        var gc = root.groupControls[i];
                        if (!gc)
                            continue;

                        if (gc.clipLayer < model.layer && model.layer <= (gc.clipLayer + gc.layerCount))
                            activeGroups.push(gc);

                    }
                    // 2. レイヤー順（親→子）にソート
                    // 上位レイヤー(数値が小さい)ほど親として振る舞う
                    activeGroups.sort(function(a, b) {
                        return a.clipLayer - b.clipLayer;
                    });
                    // 3. 行列による階層的な座標計算
                    // グローバル座標系での変換行列を作成
                    var m = Qt.matrix4x4();
                    var totalOpacity = pOpacity;
                    var totalRotX = 0;
                    var totalRotY = 0;
                    var totalRotZ = 0;
                    // 親グループから順に変換を適用
                    for (var j = 0; j < activeGroups.length; ++j) {
                        var g = activeGroups[j];
                        // AviUtl互換座標系 (Y下プラス) で計算するため、Y軸反転などは最終出力時に行う
                        m.translate(Qt.vector3d(g.x, g.y, g.z));
                        m.rotate(g.rotationX, Qt.vector3d(1, 0, 0));
                        m.rotate(g.rotationY, Qt.vector3d(0, 1, 0));
                        m.rotate(g.rotationZ, Qt.vector3d(0, 0, 1));
                        var s = g.scale / 100;
                        m.scale(s, s, s);
                        totalOpacity *= g.opacity;
                        // 回転の単純加算（近似値。厳密な3D回転合成ではないが、AviUtl的な挙動としては十分）
                        totalRotX += g.rotationX;
                        totalRotY += g.rotationY;
                        totalRotZ += g.rotationZ;
                    }
                    // 最後にオブジェクト自身のローカル座標を適用
                    m.translate(Qt.vector3d(px, py, pz));
                    // 行列から最終的な位置を取得 (translation vector)
                    // column(3) は平行移動成分 (x, y, z, w)
                    var pos = m.column(3);
                    var t = {
                        "x": pos.x,
                        "y": pos.y,
                        "z": pos.z,
                        "rx": pRotX + totalRotX,
                        "ry": pRotY + totalRotY,
                        "rz": pRotZ + totalRotZ,
                        "sx": baseScale * aspectX,
                        "sy": baseScale * aspectY,
                        "sz": baseScale,
                        "opacity": totalOpacity
                    };
                    return t;
                }

                function dbg(msg) {
                    Logger.log("[CompositeView][clipId=" + model.id + "][type=" + model.type + "] " + msg, TimelineBridge);
                }

                // レイヤーが非表示の場合は描画しない
                visible: {
                    return root.getLayerVisible(model.layer);
                }
                // 座標変換: 中心(0,0)、Y軸下プラス(AviUtl互換)
                // Qt3DはY上がプラスなので、入力を反転させる
                x: effectiveTransform.x
                y: -effectiveTransform.y
                z: effectiveTransform.z + (model.layer * 5)
                // 中心座標 (Pivot)
                pivot: Qt.vector3d(p.anchorX || 0, -(p.anchorY || 0), p.anchorZ || 0)
                // 3軸回転
                eulerRotation.x: effectiveTransform.rx
                eulerRotation.y: -effectiveTransform.ry
                eulerRotation.z: -effectiveTransform.rz
                scale.x: effectiveTransform.sx
                scale.y: effectiveTransform.sy
                scale.z: effectiveTransform.sz
                // 不透明度 (全体)
                opacity: effectiveTransform.opacity
                // params 変化 (scale/pos/rot/opacity) → BaseObject の fbCaptureItem を同期
                onPxChanged: objectContainer._syncTransformToItem()
                onPyChanged: objectContainer._syncTransformToItem()
                onPRotZChanged: objectContainer._syncTransformToItem()
                onBaseScaleChanged: objectContainer._syncTransformToItem()
                onAspectXChanged: objectContainer._syncTransformToItem()
                onAspectYChanged: objectContainer._syncTransformToItem()
                onPOpacityChanged: objectContainer._syncTransformToItem()

                // Loader (2D) は 3D シーン内では機能しないため、
                // Qt.createComponent を使用して Node 派生クラスを動的に生成する
                Common.NodeLoader {
                    id: objectContainer

                    // clipNode の transform が変わるたびに BaseObject へ同期
                    function _syncTransformToItem() {
                        if (!item)
                            return ;

                        if ("clipNodeScaleX" in item)
                            item.clipNodeScaleX = clipNode.baseScale * clipNode.aspectX;

                        if ("clipNodeScaleY" in item)
                            item.clipNodeScaleY = clipNode.baseScale * clipNode.aspectY;

                        if ("clipNodePosX" in item)
                            item.clipNodePosX = clipNode.px;

                        if ("clipNodePosY" in item)
                            item.clipNodePosY = clipNode.py;

                        if ("clipNodeRotZ" in item)
                            item.clipNodeRotZ = clipNode.pRotZ;

                        if ("clipNodeOpacity" in item)
                            item.clipNodeOpacity = clipNode.pOpacity;

                    }

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
                    // NodeLoader 完了後に fbRendererOutput を clipNode へ接続
                    onItemChanged: {
                        // FB専用プロパティは FrameBufferObject にのみ注入
                        if (item) {
                            // fbCaptureItem は BaseObject の property alias として公開済み
                            clipNode.fbRendererOutput = item.fbCaptureItem ?? null;
                            // clipLayer と sceneRootRef は FB のみが持つプロパティ
                            if (typeof item.clipLayer !== "undefined")
                                item.clipLayer = model.layer;

                            if (typeof item.sceneRootRef !== "undefined")
                                item.sceneRootRef = sceneRoot;

                            // レイヤー番号を注入 (グループ制御判定用)
                            if ("clipLayer" in item)
                                item.clipLayer = model.layer;

                            // グループ制御オブジェクトなら登録
                            if (item.isGroupControl && root.registerGroupControl)
                                root.registerGroupControl(item);

                            // 変換パラメータを注入 (初期値)
                            _syncTransformToItem();
                        }
                    }
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
