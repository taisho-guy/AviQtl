import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "common" as Common
import Qt.labs.qmlmodels 1.0
import Qt.labs.qmlmodels 1.0
import QtQml 2.15

ApplicationWindow {
    id: root
    width: 350
    height: 500
    title: "Object Settings"
    color: "#333"
    visible: true
    x: 500
    y: 200

    property int targetClipId: TimelineBridge ? TimelineBridge.selectedClipId : -1
    property var effectsModel: []
    property bool inputting: false // 入力中フラグ（バインディングループ防止用）

    // 選択変更やデータ更新を監視してモデルをリロード
    Connections {
        target: TimelineBridge
        function onSelectedClipIdChanged() { reload() }
        // パラメータ変更時の反映用（入力中はリロードしない）
        function onActiveClipsChanged() { if(!inputting) reload() }
    }

    Component.onCompleted: reload()

    function reload() {
        if (!TimelineBridge) return
        var id = TimelineBridge.selectedClipId
        targetClipId = id
        if (id >= 0) {
            effectsModel = TimelineBridge.getClipEffects(id)
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

                    // エフェクトヘッダー
                    Rectangle {
                        Layout.fillWidth: true
                        height: 24
                        color: "#444"
                        Label {
                            text: modelData.name
                            color: "white"
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
                            
                            // 親のコンテキストからデータを取得
                            property int effIdx: effectRoot.effectIndex
                            property var effVal: effectRoot.currentParams[key]
                            
                            Label { 
                                text: key
                                color: "#ccc"
                                Layout.preferredWidth: 70 
                                elide: Text.ElideRight
                            }

                            // 数値の場合: スライダー + 入力欄
                            // 文字列の場合: テキストエリア
                            Loader {
                                Layout.fillWidth: true
                                sourceComponent: typeof effVal === 'number' ? numberEditor : textEditor
                            }
                            
                            Component {
                                id: numberEditor
                                RowLayout {
                                    Slider {
                                        Layout.fillWidth: true
                                        // 簡易的な範囲設定（本来はメタデータが必要）
                                        from: (key === "scale" || key === "opacity") ? 0 : -1000
                                        to: (key === "scale") ? 500 : (key === "opacity" ? 1.0 : 1000)
                                        value: typeof effVal === 'number' ? effVal : 0
                                        
                                        onPressedChanged: {
                                            if (pressed) inputting = true
                                            else inputting = false
                                        }
                                        onMoved: {
                                            if (TimelineBridge)
                                                TimelineBridge.updateClipEffectParam(targetClipId, effIdx, key, value)
                                        }
                                    }
                                    TextField {
                                        Layout.preferredWidth: 60
                                        text: typeof effVal === 'number' ? effVal.toFixed(2) : String(effVal)
                                        selectByMouse: true
                                        onEditingFinished: {
                                            if (TimelineBridge)
                                                TimelineBridge.updateClipEffectParam(targetClipId, effIdx, key, Number(text))
                                        }
                                    }
                                }
                            }
                            
                            Component {
                                id: textEditor
                                TextField {
                                    Layout.fillWidth: true
                                    text: String(effVal)
                                    selectByMouse: true
                                    onEditingFinished: {
                                        if (TimelineBridge)
                                            TimelineBridge.updateClipEffectParam(targetClipId, effIdx, key, text)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}