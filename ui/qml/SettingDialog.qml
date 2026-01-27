import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "common" as Common
import Qt.labs.qmlmodels 1.0
import Qt.labs.qmlmodels 1.0
import QtQml 2.15

Common.RinaWindow {
    id: root
    width: 350
    height: 500
    title: "設定ダイアログ"
    color: palette.window
    visible: true
    x: 500
    y: 200

    SystemPalette { id: palette; colorGroup: SystemPalette.Active }

    property int targetClipId: TimelineBridge ? TimelineBridge.selectedClipId : -1
    property var effectsModel: []
    property bool inputting: false // 入力中フラグ（バインディングループ防止用）

    // 選択変更やデータ更新を監視してモデルをリロード
    Connections {
        target: TimelineBridge
        function onSelectedClipIdChanged() { reload() }
        // パラメータ変更時の反映用（入力中はリロードしない）
        function onSelectedClipDataChanged() { if(!inputting) reload() }
    }

    Component.onCompleted: reload()

    function reload() {
        if (!TimelineBridge) return
        var id = TimelineBridge.selectedClipId
        targetClipId = id
        if (id >= 0) {
            effectsModel = TimelineBridge.getClipEffectsModel(id)
        } else {
            effectsModel = []
        }
    }

    menuBar: MenuBar {
        Menu {
            title: "Filter"
            
            Repeater {
                model: TimelineBridge ? TimelineBridge.getAvailableEffects() : []
                MenuItem {
                    text: modelData.name
                    onTriggered: {
                        if(TimelineBridge) TimelineBridge.addEffect(targetClipId, modelData.id)
                    }
                }
            }
        }
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
                    width: root.width
                    spacing: 0
                    
                    // 親デリゲートでデータとインデックスを公開
                    property int effectIndex: index
                    property var currentParams: modelData.params
                    property var effectModel: modelData

                    // エフェクトヘッダー
                    Rectangle {
                        Layout.fillWidth: true
                        height: 24
                        color: palette.midlight
                        Label {
                            text: modelData.name
                            color: palette.windowText
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
                        model: Object.keys(modelData.params).sort()
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            Layout.margins: 4
                            property string key: modelData
                            property var effectModel: effectRoot.effectModel
                            
                            // 親のコンテキストからデータを取得
                            property int effIdx: effectRoot.effectIndex
                            property var effVal: effectRoot.currentParams[key]
                            property bool isNumber: typeof effVal === 'number'
                            
                            // トラック情報と補間状態の取得
                            property var tracks: effectModel.keyframeTracks
                            property var track: tracks ? tracks[key] : undefined
                            property bool hasKeyframes: track && track.length > 0
                            property string interpType: (hasKeyframes && track[0].interp) ? track[0].interp : "constant"
                            property bool isMoving: isNumber && interpType !== "constant" && hasKeyframes

                            // 値の取得（始点・終点）
                            property var startVal: isNumber ? effectModel.evaluatedParam(key, 0) : effVal
                            property var endVal: isNumber ? effectModel.evaluatedParam(key, TimelineBridge.clipDurationFrames) : effVal

                            // --- Left Slider (Start) ---
                            Slider {
                                visible: isNumber
                                Layout.fillWidth: true
                                from: (key === "scale" || key === "opacity") ? 0 : -1000
                                to: (key === "scale") ? 500 : (key === "opacity" ? 1.0 : 1000)
                                value: Number(startVal) || 0
                                onMoved: updateParam(0, value)
                                onPressedChanged: if (pressed) inputting = true; else inputting = false
                            }

                            // --- Left Box (Start) ---
                            TextField {
                                visible: isNumber
                                Layout.preferredWidth: 60
                                text: isNumber ? (Number(startVal) || 0).toFixed(2) : ""
                                selectByMouse: true
                                onEditingFinished: updateParam(0, Number(text))
                            }

                            // --- Center Button (Param Name & Interp Menu) ---
                            Button {
                                id: interpBtn
                                Layout.preferredWidth: 100
                                Layout.fillWidth: !isNumber
                                text: {
                                    if (!isNumber) return key
                                    switch(interpType) {
                                        case "linear": return key + " (直)";
                                        case "ease_in": return key + " (加)";
                                        case "ease_out": return key + " (減)";
                                        case "ease_in_out": return key + " (加減)";
                                        default: return key
                                    }
                                }
                                enabled: isNumber
                                onClicked: interpMenu.open()

                                Menu {
                                    id: interpMenu
                                    y: interpBtn.height
                                    MenuItem { 
                                        text: "移動なし"
                                        onTriggered: {
                                            let track = effectModel.keyframeTracks[key] || []
                                            for (let i = 0; i < track.length; i++) {
                                                effectModel.removeKeyframe(key, track[i].frame)
                                            }
                                        }
                                    }
                                    MenuSeparator {}
                                    MenuItem { text: "直線移動"; onTriggered: setInterp("linear") }
                                    MenuItem { text: "加速移動"; onTriggered: setInterp("ease_in") }
                                    MenuItem { text: "減速移動"; onTriggered: setInterp("ease_out") }
                                    MenuItem { text: "加減速移動"; onTriggered: setInterp("ease_in_out") }

                                    function setInterp(type) {
                                        let dur = TimelineBridge.clipDurationFrames
                                        let val = startVal
                                        effectModel.setKeyframe(key, 0, val, type)
                                        let eVal = isMoving ? endVal : val
                                        effectModel.setKeyframe(key, dur, eVal, "linear")
                                    }
                                }
                            }

                            // --- Right Box (End) ---
                            TextField {
                                // 常に表示するが、移動なしの時は無効化（グレーアウト）
                                enabled: isMoving
                                Layout.preferredWidth: 60
                                text: isNumber ? (Number(endVal) || 0).toFixed(2) : ""
                                selectByMouse: true
                                onEditingFinished: updateParam(TimelineBridge.clipDurationFrames, Number(text))
                            }

                            // --- Right Slider (End) ---
                            Slider {
                                enabled: isMoving
                                Layout.fillWidth: true
                                from: (key === "scale" || key === "opacity") ? 0 : -1000
                                to: (key === "scale") ? 500 : (key === "opacity" ? 1.0 : 1000)
                                value: Number(endVal) || 0
                                onMoved: updateParam(TimelineBridge.clipDurationFrames, value)
                                onPressedChanged: if (pressed) inputting = true; else inputting = false
                            }

                            // --- Text Editor (Non-numeric) ---
                            TextField {
                                visible: !isNumber
                                Layout.fillWidth: true
                                text: String(effVal)
                                selectByMouse: true
                                onEditingFinished: TimelineBridge.updateClipEffectParam(targetClipId, effIdx, key, text)
                            }

                            function updateParam(frame, val) {
                                // UIの表示更新のためにローカルプロパティを更新（バインディング遅延対策）
                                if (frame === 0) startVal = val
                                else endVal = val

                                let type = (frame === 0) ? interpType : "linear"
                                if (type === "constant") type = "linear"
                                effectModel.setKeyframe(key, frame, val, type)
                            }
                        }
                    }
                }
            }
        }
    }
}