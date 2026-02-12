import QtQuick 2.15
import QtQuick.Effects
import QtQuick3D 6.0
import "../common" as Common

Common.BaseObject {
    id: fbObject
    
    // 外部から注入されるテクスチャソース
    property var sourceTexture: null
    
    // プロジェクト設定（なければフルHD）
    readonly property int projW: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.width : 1920
    readonly property int projH: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.height : 1080
    
    // BaseObjectのsourceItemとして2Dコンテンツを設定
    sourceItem: contentItem

    // 3Dシーンへのマッピング定義 (ImageObject/RectObjectと同様)
    Model {
        source: "#Rectangle"
        // 3Dモデルへのマッピングスケール
        // renderer.output.sourceItem (エフェクト適用後) または sourceItem (元画像) のサイズを使用
        scale: Qt.vector3d(
            ((renderer.output.sourceItem ? renderer.output.sourceItem.width : fbObject.sourceItem.width) / 100), 
            ((renderer.output.sourceItem ? renderer.output.sourceItem.height : fbObject.sourceItem.height) / 100), 
            1
        )
        opacity: fbObject.opacity

        materials: DefaultMaterial {
            id: mat
            lighting: DefaultMaterial.NoLighting
            cullMode: DefaultMaterial.NoCulling
            blendMode: DefaultMaterial.SourceOver

            // テクスチャがある場合のみ使用
            diffuseMap: Texture {
                sourceItem: renderer.output
                mappingMode: Texture.UV
                tilingModeHorizontal: Texture.ClampToEdge
                tilingModeVertical: Texture.ClampToEdge
            }
        }
    }

    // 2Dコンテンツの実体 (不可視、テクスチャソース用)
    Item {
        id: contentItem
        
        // プロジェクト解像度に合わせてサイズ設定
        width: projW
        height: projH
        visible: false // BaseObjectにより制御されるが、明示的にfalse (テクスチャソースとしてのみ使用)
        
        // 1. デバッグ用の赤い背景（ソースがない場合のみ表示）
        Rectangle {
            anchors.fill: parent
            color: fbObject.sourceTexture ? "transparent" : "#80FF0000" // ソースなしなら半透明赤
            border.color: "red"
            border.width: 4
            visible: !fbObject.sourceTexture
        }
        
        // 2. 本体の描画 (ShaderEffect)
        ShaderEffect {
            anchors.fill: parent
            visible: fbObject.sourceTexture !== null
            
            // sourceTextureがShaderEffectSourceならこれで描画される
            property variant source: fbObject.sourceTexture
            
            // 上下反転対策 (ShaderEffectSourceの仕様)
            // 必要に応じて有効化
            // property bool flipY: true 
            
            // デフォルトのパススルーシェーダー (Qt Quick標準)
            // fragmentShaderを指定しない場合、sourceがそのまま描画される
        }
    }
    
    // 動作確認: ログ出力
    Component.onCompleted: {
        console.log("[FrameBuffer] Created. Source:", fbObject.sourceTexture);
    }
}
