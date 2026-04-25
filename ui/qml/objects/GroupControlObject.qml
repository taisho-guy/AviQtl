import QtQuick
import "qrc:/qt/qml/AviQtl/ui/qml/common" as Common

Common.BaseObject {
    id: root

    property bool isGroupControl: true
    // 【AviUtl完全互換パラメータ】（既存を維持）
    property int layerCount: Math.max(1, Number(evalParam("GroupControl", "layerCount", 1)))
    property real x: Number(evalParam("GroupControl", "x", 0))
    property real y: Number(evalParam("GroupControl", "y", 0))
    property real z: Number(evalParam("GroupControl", "z", 0))
    property real scale: Number(evalParam("GroupControl", "scale", 100))
    property real rotationX: Number(evalParam("GroupControl", "rotationX", 0))
    property real rotationY: Number(evalParam("GroupControl", "rotationY", 0))
    property real rotationZ: Number(evalParam("GroupControl", "rotationZ", 0))
    property real opacity: Number(evalParam("GroupControl", "opacity", 1))

    // CompositeViewへの登録・解除
    Component.onCompleted: {
        // renderHost (offscreenRenderHost) の親は CompositeView
        if (renderHost && renderHost.parent && renderHost.parent.registerGroupControl)
            renderHost.parent.registerGroupControl(root);

    }
    Component.onDestruction: {
        if (renderHost && renderHost.parent && renderHost.parent.unregisterGroupControl)
            renderHost.parent.unregisterGroupControl(root);

    }

    sourceItem: Item {
        width: 1
        height: 1
        visible: false
    }

}
