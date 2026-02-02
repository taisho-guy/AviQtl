import "../common" as Common
import QtQuick 2.15
import QtQuick3D 6.0

// SceneObject は BaseObject を使わず、直接 Node として実装
Node {
    id: root

    // CompositeView から注入されるプロパティ
    property int clipId: -1
    property int clipStartFrame: 0
    property int clipDurationFrames: 0
    property var clipParams: ({
    })
    property Item renderHost: null
    // パラメータから参照先シーンIDを取得
    property int targetSceneId: Number(evalParam("sceneId", -1))
    // ■■■ 安全装置 ■■■
    // 1. シーンIDが無効 (-1)
    // 2. 自分自身を参照している (現在の編集画面と同じID) ※完全な循環検知はC++で行うが、これは簡易ガード
    property bool isSafeToRender: {
        if (targetSceneId < 0)
            return false;

        if (TimelineBridge && targetSceneId === TimelineBridge.currentSceneId)
            return false;

        return true;
    }
    // TimelineBridge 経由でシーンのクリップを取得
    property var sceneClips: {
        if (!isSafeToRender || !TimelineBridge)
            return [];

        return TimelineBridge.getSceneClips(targetSceneId);
    }
    // Transform の取得（他オブジェクトとの一貫性のため）
    readonly property var transformParams: {
        var p = {
        };
        for (var k in clipParams) p[k] = clipParams[k]
        return p;
    }

    // Transform パラメータの取得（evalParam 互換）
    function evalParam(name, fallback) {
        if (clipParams && clipParams[name] !== undefined)
            return clipParams[name];

        return fallback;
    }

    // 隠しレンダリング用のコンテナ
    Item {
        id: offscreenContainer

        parent: renderHost // 2D空間に配置
        width: 1920
        height: 1080
        opacity: 0
        z: -9999
        visible: root.isSafeToRender // ガード: 危険な時はレンダリング自体をしない
        // layer.enabled を有効化してオフスクリーンレンダリング
        layer.enabled: true
        layer.smooth: true
        layer.textureSize: Qt.size(1920, 1080)

        Common.SceneRenderer {
            id: renderer

            anchors.fill: parent
            clipList: root.sceneClips
            // 時刻変換: 親シーンの currentFrame からこのクリップの開始位置を引く
            currentFrame: {
                if (!TimelineBridge || !TimelineBridge.transport)
                    return 0;

                var globalFrame = TimelineBridge.transport.currentFrame;
                var localFrame = globalFrame - root.clipStartFrame;
                return Math.max(0, localFrame);
            }
            projWidth: 1920
            projHeight: 1080
            renderHost: offscreenContainer
        }

    }

    // 3Dモデル（テクスチャマッピング）
    Model {
        source: "#Rectangle"
        // Transform の適用
        x: transformParams.x || 0
        y: -(transformParams.y || 0)
        z: (transformParams.z || 0) + (root.clipStartFrame * 0.01) // レイヤーの代わり
        eulerRotation.x: transformParams.rotationX || 0
        eulerRotation.y: -(transformParams.rotationY || 0)
        eulerRotation.z: -(transformParams.rotationZ || 0)
        scale: {
            var s = (transformParams.scale || 100) * 0.01;
            var a = transformParams.aspect || 0;
            return Qt.vector3d(s * (a >= 0 ? 1 + a : 1), s * (a < 0 ? 1 - a : 1), s);
        }
        opacity: transformParams.opacity !== undefined ? transformParams.opacity : 1

        materials: DefaultMaterial {
            lighting: DefaultMaterial.NoLighting
            blendMode: DefaultMaterial.SourceOver

            diffuseMap: Texture {
                sourceItem: sceneTexture
            }

        }

    }

    // シーンのテクスチャソース
    ShaderEffectSource {
        id: sceneTexture

        parent: renderHost // 2D空間に配置
        visible: false // 描画ソースなので非表示
        sourceItem: offscreenContainer
        live: true
        hideSource: true // ソース(offscreenContainer)は隠す

        // 再帰エラー時は警告を表示
        Rectangle {
            anchors.fill: parent
            color: "black"
            visible: !root.isSafeToRender
            border.color: "red"
            border.width: 4

            Text {
                anchors.centerIn: parent
                text: "Circular Reference\nor Invalid Scene"
                color: "red"
                font.pixelSize: 48
            }

        }

    }

}
