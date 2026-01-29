import QtQuick
import QtQuick3D
import QtQuick.Effects
import Rina
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Node {
    id: root
    
    property int currentFrame: TimelineBridge ? TimelineBridge.currentFrame : 0

    // 追加: 親からIDを受け取る
    property int clipId: -1
    property int clipStartFrame: 0
    property int clipDurationFrames: 0
    property int relFrame: TimelineBridge ? (TimelineBridge.currentFrame - clipStartFrame) : 0

    // 生のデータモデルを取得
    property list<QtObject> rawEffectModels: TimelineBridge && clipId >= 0 ? TimelineBridge.getClipEffectsModel(clipId) : []

    // ヘルパー関数：キーフレーム優先で評価
    function evalParam(effectId, paramName, fallback) {
        for(let i=0; i<rawEffectModels.length; i++) {
            let eff = rawEffectModels[i];
            if (eff.id === effectId && eff.enabled) {
                if (eff.evaluatedParam) {
                    var v = eff.evaluatedParam(paramName, relFrame);
                    if (v !== undefined && v !== null) return v;
                }
                return eff.params[paramName] !== undefined ? eff.params[paramName] : fallback;
            }
        }
        return fallback;
    }

    // キーフレーム対応プロパティ（rect エフェクトから取得）
    property real sizeW: evalParam("rect", "sizeW", 100)
    property real sizeH: evalParam("rect", "sizeH", 100)
    property color color: evalParam("rect", "color", "#66aa99")
    property real opacity: evalParam("rect", "opacity", 1.0)

    // エフェクトチェーン用に、オブジェクト定義(rect)やTransformを除外したリストを生成
    property var filterModels: {
        var list = [];
        for(var i=0; i<rawEffectModels.length; i++) {
            var eff = rawEffectModels[i];
            // "rect", "text", "transform" は描画フィルタではないため除外
            if (eff.id !== "rect" && eff.id !== "text" && eff.id !== "transform") {
                list.push(eff);
            }
        }
        return list;
    }

    function getBlurPadding() {
        for(let i=0; i<rawEffectModels.length; i++) {
            if(rawEffectModels[i].id === "blur" && rawEffectModels[i].enabled) {
                var v = rawEffectModels[i].evaluatedParam ? rawEffectModels[i].evaluatedParam("size", relFrame) : undefined;
                if (v === undefined || v === null) v = rawEffectModels[i].params["size"];
                return Number(v || 0);
            }
        }
        return 0;
    }
    property real padding: getBlurPadding()

    // --- テクスチャ生成パイプライン ---
    resources: [
        Item {
            id: sourceItem
            visible: true // hideSourceで隠れるのでtrueでOK
            width: root.sizeW + root.padding * 2
            height: root.sizeH + root.padding * 2

            Rectangle {
                anchors.centerIn: parent
                width: root.sizeW
                height: root.sizeH
                color: root.color
            }
        },

        Common.ObjectRenderer {
            id: renderer
            originalSource: sourceItem
            effectModels: root.filterModels
            relFrame: root.relFrame
        }
    ]

    Model {
        source: "#Rectangle"
        // 3Dモデルへのマッピング
        scale: Qt.vector3d(
            ((renderer.output.sourceItem ? renderer.output.sourceItem.width  : sourceItem.width)  / 100),
            ((renderer.output.sourceItem ? renderer.output.sourceItem.height : sourceItem.height) / 100),
            1
        )
        opacity: root.opacity
        materials: DefaultMaterial {
            diffuseMap: Texture { sourceItem: renderer.output }
            lighting: DefaultMaterial.NoLighting
            blendMode: DefaultMaterial.SourceOver
        }
    }
}
