import QtQml 2.15
import QtQuick 2.15
import QtQuick3D 6.0
import "common" as Common
import "common/Logger.js" as Logger

Item {
    id: root

    readonly property int hiddenZ: -9999

    anchors.fill: parent

    // 2Dレンダー（sourceItem/effects/ShaderEffectSource）を必ずQQuickWindow配下に置くためのホスト
    // visible:false でもWindow配下に居ればSceneGraph/Timerが正常に動く
    // 重要:
    // visible:false は SceneGraph から外れる → ShaderEffectSource の更新が止まりやすい。
    // 表示を消したいだけなら visible:true のまま opacity:0 に寄せる。
    Item {
        id: offscreenRenderHost

        anchors.fill: parent
        visible: true
        opacity: 0
        enabled: false
        z: hiddenZ
    }

    // 背景（余白部分）は黒
    Rectangle {
        anchors.fill: parent
        color: "black"
        z: -2
    }

    // プレビューコンテナ（アスペクト比維持）
    Common.SceneRenderer {
        id: mainRenderer

        // プロジェクト設定の解像度を取得（未設定時はFHD）
        property int projW: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.width : 1920
        property int projH: (TimelineBridge && TimelineBridge.project) ? TimelineBridge.project.height : 1080
        // アスペクト比計算
        property double aspect: projW / projH

        // 親に収まる最大サイズを計算 (Letterboxing)
        width: Math.min(parent.width, parent.height * aspect)
        height: Math.min(parent.height, parent.width / aspect)
        anchors.centerIn: parent
        clipList: TimelineBridge ? TimelineBridge.clipModel : null
        currentFrame: (TimelineBridge && TimelineBridge.transport) ? TimelineBridge.transport.currentFrame : 0
        projWidth: projW
        projHeight: projH
        renderHost: offscreenRenderHost
        focus: true
        Keys.onSpacePressed: {
            if (TimelineBridge && TimelineBridge.transport)
                TimelineBridge.transport.togglePlay();

        }

        // プロジェクト領域の背景 (SceneRendererは透明なので、ここで黒背景を敷く)
        Rectangle {
            anchors.fill: parent
            color: "#0a0a0a"
            z: -1
        }

    }

}
