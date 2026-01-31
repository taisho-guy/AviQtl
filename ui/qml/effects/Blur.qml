import Qt5Compat.GraphicalEffects 1.0
import QtQuick 2.15
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // QtQuick.Effects MultiEffect を捨てる（blurSrc* 警告＆ゴースト経路を遮断）
    FastBlur {
        anchors.fill: parent
        source: root.source
        // 既存パラメータ(size)をそのまま radius に割当（必要なら後でスケール調整）
        radius: Math.max(0, root.evalNumber("size", 10))
        transparentBorder: true
        visible: root.source !== null && root.width > 0 && root.height > 0
    }

}
