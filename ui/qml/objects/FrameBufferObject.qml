import QtQml
import QtQuick
import QtQuick3D
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: root

    // CompositeView から注入 (properties ではなく onItemChanged で動的セット)
    property var sceneRootRef: null
    // onItemChanged で item.clipLayer = model.layer される
    property bool clearBelow: Boolean(evalParam("frame_buffer", "clearBelow", false))
    // ObjectRenderer の Binding が要求するダミープロパティ (警告抑制)
    property var source: undefined
    property var params: ({
    })
    property var effectModel: null
    property int frame: 0
    // width/height は flattenHost のサイズで代替 (Binding が上書きしないよう明示宣言)
    property real fbWidth: flattenHost.width
    property real fbHeight: flattenHost.height
    // ─── 内部: 上位レイヤー収集 ───────────────────────────────────
    property var _capturedOutputs: []
    // sceneRootRef が設定されたら、全 clipNode の fbRendererOutput を Connections で監視する
    property var _watchConnections: []

    function _rebuildCapture() {
        if (!sceneRootRef || clipLayer < 0) {
            _capturedOutputs = [];
            return ;
        }
        var outputs = [];
        var ch = sceneRootRef.children;
        for (var i = 0; i < ch.length; i++) {
            var node = ch[i];
            var nodeLayer = (node.clipLayerRole !== undefined) ? node.clipLayerRole : -1;
            if (nodeLayer >= 0 && nodeLayer < root.clipLayer) {
                var out = node.fbRendererOutput;
                if (out)
                    outputs.push({
                    "layer": nodeLayer,
                    "src": out
                });

            }
        }
        // レイヤー昇順ソートして output だけ抽出
        outputs.sort(function(a, b) {
            return a.layer - b.layer;
        });
        var sorted = [];
        for (var j = 0; j < outputs.length; j++) sorted.push(outputs[j].src)
        _capturedOutputs = sorted;
    }

    function _startRebuildWatch() {
        // 既存の監視を全解除
        for (var i = 0; i < _watchConnections.length; i++) _watchConnections[i].destroy()
        _watchConnections = [];
        if (!sceneRootRef)
            return ;

        var ch = sceneRootRef.children;
        for (var j = 0; j < ch.length; j++) {
            var node = ch[j];
            // 各 clipNode の fbRendererOutput 変化を監視
            var conn = Qt.createQmlObject('import QtQml; Connections { function onFbRendererOutputChanged() { Qt.callLater(root._rebuildCapture); } }', root, "fbWatch");
            conn.target = node;
            _watchConnections.push(conn);
        }
        Qt.callLater(_rebuildCapture);
    }

    onSceneRootRefChanged: _startRebuildWatch()
    onClipLayerChanged: _startRebuildWatch()
    Component.onCompleted: {
        adopt2D(flattenHost);
        adopt2D(fbSourceWrapper);
        fbSourceWrapper.visible = true; // BaseObject.onSourceItemChanged が false にするため上書き
        Qt.callLater(_rebuildCapture);
    }

    Connections {
        function onClipsChanged() {
            Qt.callLater(root._rebuildCapture);
        }

        target: TimelineBridge
    }

    // ─── 合成ホスト (offscreenRenderHost へ adopt2D される) ──────
    Item {
        id: flattenHost

        width: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.width : 1920
        height: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.height : 1080
        visible: true // SceneGraph に残す (renderHost 側の opacity:0 で非表示にする)

        // 収集した各レイヤーの output をレイヤー順に重ねる
        Repeater {
            model: root._capturedOutputs

            ShaderEffectSource {
                anchors.fill: parent
                sourceItem: modelData
                live: true
                hideSource: false // View3D 側の描画はそのまま残す
                z: index
            }

        }

    }

    // ─── 3D Model として View3D に配置 ───────────────────────────
    Model {
        source: "#Rectangle"
        scale: Qt.vector3d(flattenHost.width / 100, flattenHost.height / 100, 1)

        materials: DefaultMaterial {
            lighting: DefaultMaterial.NoLighting
            blendMode: root.blendMode
            cullMode: root.cullMode

            diffuseMap: Texture {
                sourceItem: renderer.output
            }

        }

    }

    // clearBelow: 下位レイヤーを黒でマスク
    Rectangle {
        visible: root.clearBelow
        anchors.fill: parent
        color: "black"
        z: -1
    }

    // flattenHost を1枚のテクスチャに焼く → sourceItem
    sourceItem: Item {
        id: fbSourceWrapper

        width: flattenHost.width
        height: flattenHost.height
        visible: true // visible:false だと ShaderEffectSource の更新が止まる

        ShaderEffectSource {
            anchors.fill: parent
            sourceItem: flattenHost
            live: true
            hideSource: true
        }

    }

}
