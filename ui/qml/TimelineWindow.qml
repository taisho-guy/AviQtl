import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "common" as Common

Common.RinaWindow {
    id: timelineWindow
    width: 1280
    height: 300
    // x: 100; y: 500 // Waylandでは無視されることが多い
    title: "Timeline [Layer 1-100]"
    visible: true

    // コンテキストメニューの定義
    Menu {
        id: contextMenu
        
        // メニューが開かれたときのマウス位置（タイムライン上の座標）を保持
        property int clickFrame: 0
        property int clickLayer: 0

        MenuItem {
            text: "Add Text Object"
            onTriggered: {
                // C++側に生成リクエストを送る
                if (TimelineBridge) {
                    TimelineBridge.createObject("text", contextMenu.clickFrame, contextMenu.clickLayer)
                }
            }
        }
        MenuItem {
            text: "Add Shape Object (Rectangle)"
            onTriggered: {
                if (TimelineBridge) {
                    TimelineBridge.createObject("rect", contextMenu.clickFrame, contextMenu.clickLayer)
                }
            }
        }
    }




    // 設定ウィンドウの参照を保持するプロパティ
    property var settingDialog: null

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ツールバー等は省略し、直接タイムラインエリアへ

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            
            Flickable {
                contentWidth: 2000
                contentHeight: 3000
                interactive: true

                // レイヤー区切り線
                Repeater {
                    model: 20
                    Rectangle {
                        y: index * 30
                        width: parent.width
                        height: 1
                        color: "#333"
                    }
                }

                // === Playhead (Current Time) ===
                Rectangle {
                    id: playhead
                    x: TimelineBridge ? TimelineBridge.currentTime : 0
                    y: 0
                    width: 2
                    height: parent.height
                    color: "red"
                    z: 100 // 最前面

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -5 // 掴みやすくする
                        cursorShape: Qt.SizeHorCursor
                        drag.target: playhead
                        drag.axis: Drag.XAxis
                        drag.minimumX: 0
                        
                        onPositionChanged: {
                            if (drag.active && TimelineBridge) {
                                TimelineBridge.currentTime = playhead.x
                            }
                        }
                    }
                }

                // 全体を覆うマウスエリア（オブジェクトがない場所のクリック用）
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton | Qt.LeftButton
                    z: -1 // オブジェクトより奥に配置

                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            // クリック位置からフレームとレイヤーを計算
                            // zoomLevelが未定義のため、一旦1.0fで計算
                            var frame = Math.floor(mouse.x / 1.0) // TODO: timelineWindow.zoomLevel を使用
                            var layer = Math.floor(mouse.y / 30) // 30px per layer
                            
                            // メニューに情報を渡して表示
                            contextMenu.clickFrame = frame
                            contextMenu.clickLayer = layer
                            contextMenu.popup()
                        }
                    }
                }

                // クリップ一覧表示 (Repeater)
                Repeater {
                    model: TimelineBridge ? TimelineBridge.clips : []
                    delegate: Rectangle {
                        id: clipRect
                        // Layer 0 is at y=0. Each layer is 30px height.
                        y: modelData.layer * 30 
                        x: modelData.start
                        width: modelData.duration
                        height: 28
                        color: "#66aa99"
                        border.color: "white"
                        border.width: 1
                        opacity: 0.8
                        radius: 4
                        
                        Text {
                            anchors.centerIn: parent
                            text: modelData.type
                            color: "white"
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                console.log("Selected clip ID:", modelData.id)
                                // TODO: Select clip in controller
                            }
                            onDoubleClicked: openSettingDialog()
                        }
                    }
                }
            }
        }
    }

    // 設定ダイアログを開く関数
    function openSettingDialog() {
        // すでに開いていたら手前に持ってくる
        if (settingDialog) {
            settingDialog.raise()
            settingDialog.requestActivate()
            return
        }

        // 動的にコンポーネントを作成
        // 同じディレクトリ(qrc:/qml/)にあるSettingDialog.qmlを読み込む
        var component = Qt.createComponent("SettingDialog.qml")
        if (component.status === Component.Ready) {
            // ウィンドウを生成。親はnullにすることで独立したトップレベルウィンドウになる
            settingDialog = component.createObject(null)
            
            // ウィンドウが閉じられたら参照を消す
            settingDialog.closing.connect(function() {
                settingDialog = null
            })
        } else {
            console.error("Error loading component:", component.errorString())
        }
    }
}