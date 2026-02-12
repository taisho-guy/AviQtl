import QtQuick 2.15
import QtQuick3D 6.0
import "../common" as Common

Common.BaseObject {
    id: fbObject
    
    // 外部から注入されるテクスチャソース
    property var sourceTexture: null
    
    // プロジェクト設定（なければフルHD）
    readonly property int projW: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.width : 1920
    readonly property int projH: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.height : 1080
    
    Model {
        // 平面プリミティブを使用
        source: "#Rectangle"
        
        // 【重要】サイズ調整
        // QtQuick3Dの#Rectangleは 100x100 単位が基本。
        // プロジェクトサイズに合わせるためスケーリング
        scale: Qt.vector3d(projW / 100, projH / 100, 1)
        
        // Zファイティング（ちらつき）防止のため、ごくわずかに手前に出す
        position: Qt.vector3d(0, 0, 0.1)
        
        materials: DefaultMaterial {
            id: mat
            lighting: DefaultMaterial.NoLighting
            cullMode: DefaultMaterial.NoCulling // 裏面も描画（念のため）
            
            // 【修正】テクスチャがある場合のみ使用
            diffuseMap: fbObject.sourceTexture ? textureParams : null
            
            // テクスチャがない場合、デバッグ用に「半透明の赤」を表示
            diffuseColor: fbObject.sourceTexture ? "white" : "#80FF0000"
            
            // テクスチャパラメータ設定
            Texture {
                id: textureParams
                sourceItem: fbObject.sourceTexture
                mappingMode: Texture.UV
                tilingModeHorizontal: Texture.ClampToEdge
                tilingModeVertical: Texture.ClampToEdge
            }
            
            // アルファブレンド有効化
            blendMode: DefaultMaterial.SourceOver
        }
    }
    
    // 動作確認: ログ出力
    Component.onCompleted: {
        console.log("[FrameBuffer] Created. Source:", fbObject.sourceTexture);
    }
}
