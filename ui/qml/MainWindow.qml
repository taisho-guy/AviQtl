import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import Qt.labs.platform 1.1 as Platform
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

    // アクション定義 (ショートカット用)
    Action {
        id: saveAction
        text: "プロジェクトを保存..."
        shortcut: "Ctrl+S"
        onTriggered: saveDialog.open()
    }
    Action {
        id: loadAction
        text: "プロジェクトを開く..."
        shortcut: "Ctrl+O"
        onTriggered: loadDialog.open()
    }
    Action {
        id: quitAction
        text: "終了"
        shortcut: "Ctrl+Q"
        onTriggered: if (WindowManager) WindowManager.requestQuit()
    }
    Action {
        id: undoAction
        text: "元に戻す"
        shortcut: "Ctrl+Z"
        onTriggered: if (TimelineBridge) TimelineBridge.undo()
    }
    Action {
        id: redoAction
        text: "やり直す"
        shortcut: "Ctrl+Shift+Z"
        onTriggered: if (TimelineBridge) TimelineBridge.redo()
    }

    menuBar: MenuBar {
        Menu {
            title: "ウィンドウ"
            MenuItem {
                text: "タイムライン"
                checkable: true
                checked: WindowManager ? WindowManager.timelineVisible : false
                onTriggered: if (WindowManager) WindowManager.timelineVisible = checked
            }
            MenuItem {
                text: "プロジェクト設定..."
                checkable: true
                checked: WindowManager ? WindowManager.projectSettingsVisible : false
                onTriggered: if (WindowManager) WindowManager.projectSettingsVisible = checked
            }
            MenuItem {
                text: "設定ダイアログ"
                checkable: true
                checked: WindowManager ? WindowManager.objectSettingsVisible : false
                onTriggered: if (WindowManager) WindowManager.objectSettingsVisible = checked
            }
        }
        Menu {
            title: "編集"
            MenuItem { action: undoAction }
            MenuItem { action: redoAction }
        }
        Menu {
            title: "ファイル"
            MenuItem { action: saveAction }
            MenuItem { action: loadAction }
            MenuItem { action: quitAction }
        }
    }

    Platform.FileDialog {
        id: saveDialog
        fileMode: Platform.FileDialog.SaveFile
        nameFilters: ["Rina Project files (*.rina)", "JSON files (*.json)"]
        defaultSuffix: "rina"
        onAccepted: {
            if (TimelineBridge) TimelineBridge.saveProject(file)
        }
    }

    Platform.FileDialog {
        id: loadDialog
        fileMode: Platform.FileDialog.OpenFile
        nameFilters: ["Rina Project files (*.rina)", "JSON files (*.json)"]
        onAccepted: {
            if (TimelineBridge) TimelineBridge.loadProject(file)
        }
    }

    // 本体を閉じたら全部消す（＝全終了）
    onClosing: (close) => {
        if (WindowManager) WindowManager.requestQuit()
        close.accepted = true
    }

    CompositeView { anchors.fill: parent }
}