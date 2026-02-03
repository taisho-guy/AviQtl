import QtQuick
import QtQuick3D
import QtMultimedia
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: base

    property string videoPath: String(evalParam("video", "path", ""))
    property real videoOpacity: Number(evalParam("video", "opacity", 1))
    property bool mute: Boolean(evalParam("video", "mute", false))
    property int startOffset: Number(evalParam("video", "startOffset", 0)) // 動画の開始位置オフセット(ms)
    
    // 3Dシーンへのマッピング
    Model {
        source: "#Rectangle"
        // アスペクト比はコンテナ(renderer.output)のサイズに基づく
        scale: Qt.vector3d(
            (renderer.output.sourceItem ? renderer.output.sourceItem.width : base.sourceItem.width) / 100, 
            (renderer.output.sourceItem ? renderer.output.sourceItem.height : base.sourceItem.height) / 100, 
            1
        )
        opacity: base.videoOpacity

        materials: DefaultMaterial {
            lighting: DefaultMaterial.NoLighting
            blendMode: DefaultMaterial.SourceOver
            diffuseMap: Texture {
                sourceItem: renderer.output
            }
        }
    }

    // 同期ロジックの中枢
    MediaPlayer {
        id: player
        source: base.videoPath
        audioOutput: AudioOutput {
            muted: base.mute || !TimelineBridge.transport.isPlaying // 停止中はミュート（スクラブ音防止）
            volume: 1.0
        }
        
        // 自動再生は無効。TimelineBridgeの制御に従う。
        autoPlay: false 

        // プロジェクトFPSを取得（デフォルト30）
        readonly property double fps: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.fps : 30.0
        
        // 現在のフレームに対応する動画内の時間(ms)を計算
        // relFrame: クリップ先頭からのフレーム数
        // startOffset: 動画ファイルの何ミリ秒目から使うか
        readonly property int targetPosition: Math.max(0, (base.relFrame / fps) * 1000 + base.startOffset)

        // フレーム変更時の同期処理
        onTargetPositionChanged: {
            // 再生中は自然な再生に任せるが、ズレ補正のために定期的チェックも検討可能。
            // ここでは簡易的に「シーク時（停止時）は即座に合わせる」実装とする。
            if (!TimelineBridge.transport.isPlaying) {
                 player.position = targetPosition
            }
        }
        
        // 再生状態の同期
        Connections {
            target: TimelineBridge.transport
            function onIsPlayingChanged() {
                if (TimelineBridge.transport.isPlaying) {
                    // 再生開始時に位置を合わせてスタート
                    var currentPos = (base.relFrame / player.fps) * 1000 + base.startOffset
                    if (Math.abs(player.position - currentPos) > 100) {
                        player.position = currentPos
                    }
                    player.play()
                } else {
                    player.pause()
                    // 停止時は正確なフレーム位置に合わせる
                    player.position = (base.relFrame / player.fps) * 1000 + base.startOffset
                }
            }
        }
    }

    // レンダリングソース（コンテナ方式）
    sourceItem: containerItem

    Item {
        id: containerItem
        
        // ぼかし等のためのパディング確保
        readonly property real pad: base.padding * 2
        
        // 動画サイズが不明な場合はデフォルト1920x1080、ロード後は動画サイズに合わせる
        // metaData.resolution は Qt6.5+ で利用可能。古い場合は implicitWidth/Height を参照
        property real videoW: (player.metaData && player.metaData.resolution) ? player.metaData.resolution.width : 1920
        property real videoH: (player.metaData && player.metaData.resolution) ? player.metaData.resolution.height : 1080
        
        // メタデータが取れない場合のフォールバック（VideoOutputのサイズ）
        width: (videoOutput.contentRect.width > 0 ? videoOutput.contentRect.width : 1920) + pad
        height: (videoOutput.contentRect.height > 0 ? videoOutput.contentRect.height : 1080) + pad
        
        visible: false // テクスチャ化されるため非表示

        VideoOutput {
            id: videoOutput
            anchors.centerIn: parent
            
            // パディングを除いた実サイズ
            width: parent.width - containerItem.pad
            height: parent.height - containerItem.pad
            
            // MediaPlayerの出力を受け取る
            // Qt6では source プロパティは廃止され、MediaPlayerを割り当てる方式に変更されたが、
            // VideoOutput { } 内では通常 `connection` は不要で、QML上はプロパティが見えない場合がある。
            // Qt6での正式な接続方法は VideoOutput の外から `player.videoOutput = videoOutput` とする等の方法もあるが、
            // 一般的には `VideoOutput` に `id` を振り、これを使う。
            // ★Qt6のVideoOutputには `source` プロパティがない場合があるため、
            // MediaPlayer側でvideoOutputプロパティを設定するのが確実。
        }
        
        // 相互リンク（VideoOutput <-> MediaPlayer）
        Component.onCompleted: {
            player.videoOutput = videoOutput
        }
    }
}
