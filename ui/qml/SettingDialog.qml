import Qt.labs.qmlmodels
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "common" as Common

Common.RinaWindow {
    id: root

    property int targetClipId: (TimelineBridge && TimelineBridge.selection) ? TimelineBridge.selection.selectedClipId : -1
    property var effectsModel: []
    property bool inputting: false // 入力中フラグ（バインディングループ防止用）

    Component {
        id: easingDialogComponent
        Common.EasingConfigDialog {}
    }
    function reload() {
        if (!TimelineBridge || !TimelineBridge.selection)
            return ;

        var id = TimelineBridge.selection.selectedClipId;
        targetClipId = id;
        if (id >= 0)
            effectsModel = TimelineBridge.getClipEffectsModel(id);
        else
            effectsModel = [];
    }

    width: 350
    height: 500
    title: "設定ダイアログ"
    color: palette.window
    visible: true
    x: 500
    y: 200
    Component.onCompleted: reload()

    // 選択変更やデータ更新を監視してモデルをリロード
    Connections {
        function onSelectedClipIdChanged() {
            reload();
        }

        // パラメータ変更時の反映用（入力中はリロードしない）
        function onSelectedClipDataChanged() {
            if (!inputting)
                reload();

        }

        target: TimelineBridge ? TimelineBridge.selection : null
    }

    Connections {
        function onClipEffectsChanged(clipId) {
            if (clipId === targetClipId)
                reload();

        }

        target: TimelineBridge
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: parent.width
        clip: true

        ColumnLayout {
            width: root.width
            spacing: 1

            Repeater {
                model: effectsModel

                delegate: ColumnLayout {
                    id: effectRoot

                    // 親デリゲートでデータとインデックスを公開
                    property int effectIndex: index
                    property var currentParams: modelData.params
                    property var effectModel: modelData

                    width: root.width
                    spacing: 0

                    // エフェクトヘッダー
                    Rectangle {
                        Layout.fillWidth: true
                        height: 24
                        color: palette.midlight

                        Label {
                            text: modelData.name
                            color: palette.text
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                        }

                        // 削除ボタン (Transform以外)
                        Button {
                            text: "×"
                            visible: modelData.id !== "transform" && modelData.id !== "rect" && modelData.id !== "text"
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            onClicked: TimelineBridge.removeEffect(targetClipId, effectRoot.effectIndex)
                        }

                    }

                    // パラメータリスト
                    // Mapを配列に変換してRepeaterに渡す
                    Repeater {
                        // End RowLayout

                        model: Object.keys(modelData.params).sort()

                        delegate: ColumnLayout {
                            property string key: modelData
                            property var effectModel: effectRoot.effectModel
                            // 親のコンテキストからデータを取得
                            property int effIdx: effectRoot.effectIndex
                            property var effVal: effectRoot.currentParams[key]
                            property bool isNumber: typeof effVal === 'number'
                            // トラック情報と補間状態の取得
                            property int curRelFrame: (TimelineBridge && TimelineBridge.transport) ? (TimelineBridge.transport.currentFrame - TimelineBridge.clipStartFrame) : 0
                            property int clipDur: TimelineBridge ? TimelineBridge.clipDurationFrames : 100
                            property var tracks: effectModel.keyframeTracks
                            property var track: tracks ? tracks[key] : undefined
                            property var kfs: track || []
                            property bool hasKeyframes: kfs.length > 0
                            property var interval: findInterval(kfs, curRelFrame, clipDur)
                            property int startFrame: interval.start
                            property int endFrame: interval.end
                            // 値の取得（始点・終点）
                            // tracksを依存関係に含めて再評価をトリガー
                            // effVal (currentParams) を依存関係に含めることで、Undo/Redo時のベース値変更にも追従させる
                            property var startVal: isNumber ? (tracks, effVal, effectModel.evaluatedParam(key, startFrame)) : effVal
                            property var endVal: isNumber ? (tracks, effVal, effectModel.evaluatedParam(key, endFrame)) : effVal
                            property string interpType: hasKeyframes ? getInterpAt(startFrame) : "constant"
                            property bool isMoving: isNumber && (hasKeyframes || interpType !== "constant")

                            function findInterval(kfs, cur, totalDur) {
                                let s = 0, e = totalDur;
                                if (!kfs || kfs.length === 0)
                                    return {
                                    "start": s,
                                    "end": e
                                };

                                let foundStart = false;
                                for (let i = kfs.length - 1; i >= 0; i--) {
                                    if (kfs[i].frame <= cur) {
                                        s = kfs[i].frame;
                                        foundStart = true;
                                        if (i + 1 < kfs.length)
                                            e = kfs[i + 1].frame;
                                        else
                                            e = totalDur;
                                        break;
                                    }
                                }
                                if (!foundStart) {
                                    e = kfs[0].frame;
                                    s = 0;
                                }
                                return {
                                    "start": s,
                                    "end": e
                                };
                            }

                            function getInterpAt(f) {
                                if (!kfs)
                                    return "linear";

                                for (var i = 0; i < kfs.length; i++) {
                                    if (kfs[i].frame === f)
                                        return kfs[i].interp || "linear";

                                }
                                return "linear";
                            }

                            function updateParam(frame, val) {
                                let type = "linear";
                                if (frame === startFrame)
                                    type = getInterpAt(startFrame);
                                else
                                    type = getInterpAt(frame);
                                if (type === "constant")
                                    type = "linear";

                                effectModel.setKeyframe(key, frame, val, { "interp": type });
                            }

                            Layout.fillWidth: true
                            spacing: 0

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.margins: 4

                                // --- Left Slider (Start) ---
                                Slider {
                                    visible: isNumber
                                    Layout.fillWidth: true
                                    from: (key === "scale" || key === "opacity") ? 0 : -1000
                                    to: (key === "scale") ? 500 : (key === "opacity" ? 1 : 1000)
                                    value: Number(startVal) || 0
                                    onMoved: updateParam(startFrame, value)
                                    onPressedChanged: {
                                        if (pressed)
                                            inputting = true;
                                        else
                                            inputting = false;
                                    }
                                }

                                // --- Left Box (Start) ---
                                TextField {
                                    visible: isNumber
                                    Layout.preferredWidth: 60
                                    text: isNumber ? (Number(startVal) || 0).toFixed(2) : ""
                                    selectByMouse: true
                                    onEditingFinished: updateParam(startFrame, Number(text))
                                }

                                // --- Center Button (Param Name & Interp Menu) ---
                                Button {
                                    id: interpBtn

                                    Layout.preferredWidth: 100
                                    Layout.fillWidth: !isNumber
                                    text: {
                                        if (!isNumber)
                                            return key;

                                        switch (interpType) {
                                        case "linear":
                                            return key + " (直)";
                                        case "ease_in":
                                            return key + " (加)";
                                        case "ease_out":
                                            return key + " (減)";
                                        case "ease_in_out":
                                            return key + " (加減)";
                                        default:
                                            return key;
                                        }
                                    }
                                    enabled: isNumber
                                    onClicked: {
                                        // Ensure at least one keyframe exists to edit
                                        if (!hasKeyframes) {
                                            effectModel.setKeyframe(key, startFrame, startVal, { "interp": "linear" });
                                            effectModel.setKeyframe(key, endFrame, endVal, { "interp": "linear" });
                                        }
                                        var dialog = easingDialogComponent.createObject(root, {
                                            "effectModel": effectModel,
                                            "paramName": key,
                                            "keyframeFrame": startFrame
                                        });
                                        dialog.open();
                                    }

                                }

                                // --- Right Box (End) ---
                                TextField {
                                    // 常に表示するが、移動なしの時は無効化（グレーアウト）
                                    enabled: isMoving
                                    Layout.preferredWidth: 60
                                    text: isNumber ? (Number(endVal) || 0).toFixed(2) : ""
                                    selectByMouse: true
                                    onEditingFinished: updateParam(endFrame, Number(text))
                                }

                                // --- Right Slider (End) ---
                                Slider {
                                    enabled: isMoving
                                    Layout.fillWidth: true
                                    from: (key === "scale" || key === "opacity") ? 0 : -1000
                                    to: (key === "scale") ? 500 : (key === "opacity" ? 1 : 1000)
                                    value: Number(endVal) || 0
                                    onMoved: updateParam(endFrame, value)
                                    onPressedChanged: {
                                        if (pressed)
                                            inputting = true;
                                        else
                                            inputting = false;
                                    }
                                }

                                // --- Text Editor (Non-numeric) ---
                                TextField {
                                    visible: !isNumber
                                    Layout.fillWidth: true
                                    text: String(effVal)
                                    selectByMouse: true
                                    onEditingFinished: TimelineBridge.updateClipEffectParam(targetClipId, effIdx, key, text)
                                }

                            }

                            // --- Mini Timeline Bar ---
                            Item {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 10
                                Layout.leftMargin: 4
                                Layout.rightMargin: 4
                                visible: isNumber

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: parent.width
                                    height: 2
                                    color: "#555"

                                    // Highlight active interval
                                    Rectangle {
                                        height: 4
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: "#4a9eff"
                                        opacity: 0.7
                                        x: (startFrame / clipDur) * parent.width
                                        width: Math.max(0, ((endFrame - startFrame) / clipDur) * parent.width)
                                        visible: clipDur > 0
                                    }

                                }

                                // Keyframes
                                Repeater {
                                    model: kfs

                                    Rectangle {
                                        width: 4
                                        height: 4
                                        radius: 2
                                        color: "white"
                                        anchors.verticalCenter: parent.verticalCenter
                                        x: (modelData.frame / clipDur) * parent.width - 2

                                        MouseArea {
                                            anchors.fill: parent
                                            acceptedButtons: Qt.RightButton
                                            onClicked: effectModel.removeKeyframe(key, modelData.frame)
                                        }

                                    }

                                }

                                // Cursor
                                Rectangle {
                                    width: 1
                                    height: parent.height
                                    color: "red"
                                    x: (curRelFrame / clipDur) * parent.width
                                    visible: clipDur > 0
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton
                                    onDoubleClicked: (mouse) => {
                                        let f = Math.round((mouse.x / width) * clipDur);
                                        let val = effectModel.evaluatedParam(key, f);
                                        effectModel.setKeyframe(key, f, val, { "interp": "linear" });
                                    }
                                }

                            }

                        }

                    }

                }

            }

        }

    }

    menuBar: MenuBar {
        Menu {
            title: "フィルタ"

            Repeater {
                model: TimelineBridge ? TimelineBridge.getAvailableEffects() : []

                MenuItem {
                    text: modelData.name
                    onTriggered: {
                        if (TimelineBridge) {
                            TimelineBridge.addEffect(targetClipId, modelData.id);
                            reload();
                        }
                    }
                }

            }

        }

    }

}
