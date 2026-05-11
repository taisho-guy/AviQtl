// SceneRenderer: Filament の描画面を QML に埋め込むラッパー。
// Phase 1: FilamentCanvas (ヘッドレス SwapChain + QSGSimpleTextureNode blit) で
//          Wayland ネイティブ描画を実現する。

import AviQtl.Rendering 1.0
import AviQtl.UI 1.0
import QtQuick

// Phase 2: CoreBridge.currentFrame を FilamentCanvas.currentFrame に接続し、
//          タイムラインのシーク・再生がプレビューに反映されるようにする。
Item {
    id: root

    property int sceneId: -1
    // CoreBridge.currentFrame を直接バインドし、QML 側で計算なし
    property int currentFrame: CoreBridge.currentFrame

    FilamentCanvas {
        anchors.fill: parent
        sceneId: root.sceneId
        currentFrame: root.currentFrame
    }

}
