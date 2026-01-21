import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import "common" as Common

ApplicationWindow {
    id: mainWin
    visible: true
    width: 640
    height: 360
    x: 100
    y: 100
    title: "Rina Main"

    // 重要: View3D の背後に黒背景を強制
    background: Rectangle { color: "#000000" }

    menuBar: MenuBar {
        Menu {
            title: "Window"
            MenuItem {
                text: "Timeline"
                checkable: true
                checked: WindowManager ? WindowManager.timelineVisible : false
                onTriggered: if (WindowManager) WindowManager.timelineVisible = checked
            }
            MenuItem {
                text: "Project Settings"
                checkable: true
                checked: WindowManager ? WindowManager.projectSettingsVisible : false
                onTriggered: if (WindowManager) WindowManager.projectSettingsVisible = checked
            }
            MenuItem {
                text: "Object Settings"
                checkable: true
                checked: WindowManager ? WindowManager.objectSettingsVisible : false
                onTriggered: if (WindowManager) WindowManager.objectSettingsVisible = checked
            }
        }
        Menu {
            title: "File"
            MenuItem { text: "Quit"; onTriggered: if (WindowManager) WindowManager.requestQuit() }
        }
    }

    // 本体を閉じたら全部消す（＝全終了）
    onClosing: (close) => {
        if (WindowManager) WindowManager.requestQuit()
        close.accepted = true
    }

    CompositeView { anchors.fill: parent }
}