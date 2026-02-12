import QtQml 2.15
import QtQuick 2.15
import QtQuick3D 6.0

// 3Dベースのレンダリングパス管理ノード
Node {
    id: root

    // このパスに含まれるレイヤーモデル
    required property var layerModels
    // 前のパスの合成結果（テクスチャ）
    property var prevPassTexture: null
    // 2D要素のホスト
    property Item renderHost: null

    Instantiator {
        // FrameBuffer用

        id: layerInstantiator

        model: root.layerModels
        onObjectAdded: (index, object) => {
            if (object) {
                object.parent = root;
                // z値の安全な設定
                if (object.hasOwnProperty("z")) {
                    var m = root.layerModels.get ? root.layerModels.get(index) : root.layerModels[index];
                    object.z = (m && m.z !== undefined) ? m.z : 0;
                }
            }
        }
        onObjectRemoved: (index, object) => {
            if (object)
                object.destroy();

        }

        delegate: Loader {
            id: objectLoader

            // 安全なモデル参照
            readonly property var safeModel: modelData || {
            }
            readonly property string objectType: safeModel.type || "unknown"
            // プロパティマップ
            property var properties: ({
                "clipId": safeModel.clipId !== undefined ? safeModel.clipId : -1,
                "clipStartFrame": safeModel.startFrame || 0,
                "clipDurationFrames": safeModel.duration || 0,
                "clipParams": safeModel.params || {
                },
                "rawEffectModels": safeModel.effects || [],
                "renderHost": root.renderHost,
                "sourceTexture": (objectType === "FrameBuffer") ? root.prevPassTexture : undefined
            })

            // 【修正箇所】三項演算子内のブロック{}を削除
            source: objectType === "FrameBuffer" ? "../objects/FrameBufferObject.qml" : "../common/NodeLoader.qml"
            asynchronous: false
            onLoaded: {
                if (item) {
                    // プロパティの手動注入
                    var keys = Object.keys(properties);
                    for (var i = 0; i < keys.length; i++) {
                        var key = keys[i];
                        if (item.hasOwnProperty(key))
                            item[key] = properties[key];

                    }
                }
            }
        }

    }

}
