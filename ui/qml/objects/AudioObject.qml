import QtQuick
import QtMultimedia
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: root

    // ObjectRenderer からのプロパティ要求に対応（ダミー定義）
    property string source: ""
    property var params: ({})
    property var effectModel: null
    property int frame: 0

    // JSONで定義したパラメータを取得 (第1引数のeffectIdはBaseObject内では無視されますが、慣習として指定)
    property string sourcePath: String(evalParam("audio", "source", ""))
    property real volume: Number(evalParam("audio", "volume", 1.0))
    property real pan: Number(evalParam("audio", "pan", 0.0))
    property bool mute: evalParam("audio", "mute", false)

    // プレビュー再生用のオーディオ出力
    MediaPlayer {
        id: player
        source: root.sourcePath
        audioOutput: AudioOutput {
            volume: root.mute ? 0 : root.volume
            // Qt6のAudioOutputにはpanプロパティがないため、必要ならShaderEffect等で実装するか、
            // 将来的にAudioDecoderで処理することになります。
        }
    }

    // 音声オブジェクトは画面に描画されないため、ダミーの不可視Itemを指定
    sourceItem: Item {
        width: 1; height: 1
        visible: false
    }

    // タイムラインのシークに合わせた再生制御
    Connections {
        target: (TimelineBridge && TimelineBridge.transport) ? TimelineBridge.transport : null

        function onIsPlayingChanged() {
            if (TimelineBridge.transport.isPlaying) {
                // 現在のフレームがクリップの範囲内かチェック
                var currentFrame = TimelineBridge.transport.currentFrame;
                if (currentFrame >= root.clipStartFrame && currentFrame < (root.clipStartFrame + root.clipDurationFrames)) {
                    // クリップ先頭からの経過時間を計算 (秒)
                    var offsetSeconds = (currentFrame - root.clipStartFrame) / TimelineBridge.project.fps;
                    
                    // ミリ秒に変換してシーク＆再生
                    player.position = offsetSeconds * 1000;
                    player.play();
                }
            } else {
                player.pause();
            }
        }
    }
}