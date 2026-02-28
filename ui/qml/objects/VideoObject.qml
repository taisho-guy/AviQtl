import QtQuick
import QtQuick.Effects
import QtQuick3D
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: base

    // パラメータ
    property string path: String(evalParam("video", "path", ""))
    property real startOffset: Number(evalParam("video", "startOffset", 0))
    property bool useFrameEasing: Boolean(evalParam("video", "useFrameEasing", false))
    property real speed: Number(evalParam("video", "speed", 100))
    property real opacity: Number(evalParam("video", "opacity", 1))
    // 内部状態
    property int updateCounter: 0
    property int currentVideoFrame: 0

    // フレーム計算ロジック
    // useFrameEasing = true:  フレーム番号自体をイージング (0% -> 100% で動画全編を再生など)
    // useFrameEasing = false: 再生速度をイージング (100% -> 200% で倍速再生など)
    function updateCurrentFrame() {
        if (useFrameEasing) {
            // フレーム指定モード: speed パラメータを「現在のフレーム位置(%)」として扱う
            // 0 = 先頭, 100 = 末尾
            // ※ startOffset は無視するか、オフセットとして加算するかは仕様次第だが、
            //   ここではシンプルに「動画全体の何％位置か」とする
            //   (必要に応じて startOffset を加味するロジックに変更可)
            var totalFrames = 1000;
            // TODO: 実際の動画総フレーム数を取得する手段が必要
            currentVideoFrame = Math.floor(totalFrames * (speed / 100));
        } else {
            // 速度指定モード: (現在のクリップ内フレーム + startOffset) * (speed / 100)
            // relFrame は BaseObject で計算される「クリップ先頭からの経過フレーム」
            currentVideoFrame = Math.floor((base.relFrame * (speed / 100)) + startOffset);
        }
    }

    onRelFrameChanged: updateCurrentFrame()
    onSpeedChanged: updateCurrentFrame()
    onStartOffsetChanged: updateCurrentFrame()
    onUseFrameEasingChanged: updateCurrentFrame()

    Connections {
        function onFrameUpdated(clipId) {
            if (clipId === base.clipId)
                base.updateCounter++;

        }

        target: TimelineBridge ? TimelineBridge.mediaManager : null
    }

    sourceItem: Item {
        width: 1920 // 仮サイズ。実際はテクスチャサイズに合わせるべき
        height: 1080
        visible: false

        Image {
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            cache: false
            source: base.clipId > 0 ? "image://videoFrame/" + base.clipId + "?f=" + base.currentVideoFrame + "&u=" + base.updateCounter : ""
            opacity: base.opacity
        }

    }

}
