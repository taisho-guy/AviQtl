import Qt.labs.platform 1.1 as Platform
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import "common" as Common

ApplicationWindow {
    id: mainWin

    visible: true
    width: 640
    height: 360
    x: 100
    y: 100
    title: "Rina - プレビュー"
    // 本体を閉じたら全部消す（＝全終了）
    onClosing: (close) => {
        if (WindowManager)
            WindowManager.requestQuit();

        close.accepted = true;
    }

    // アクション定義 (ショートカット用)
    Action {
        id: newAction

        text: "新規プロジェクト"
        shortcut: "Ctrl+N"
        onTriggered: console.log("New Project")
    }

    Action {
        id: saveProjectAction // プロジェクトの上書き保存用アクション

        text: "プロジェクトの上書き保存"
        shortcut: "Ctrl+S"
        onTriggered: {
            if (TimelineBridge) TimelineBridge.saveProject("");
        }
    }

    Action {
        id: loadAction

        text: "プロジェクトを開く"
        shortcut: "Ctrl+O"
        onTriggered: loadDialog.open()
    }

    Action {
        id: saveAsProjectAction // プロジェクトを名前を付けて保存用アクション

        text: "プロジェクトを名前を付けて保存..."
        shortcut: "Ctrl+Shift+S"
        onTriggered: saveDialog.open()

    }

    Action {
        id: quitAction

        text: "終了"
        shortcut: "Ctrl+Q"
        onTriggered: {
            if (WindowManager)
                WindowManager.requestQuit();

        }
    }

    Action {
        id: undoAction

        text: "元に戻す"
        shortcut: "Ctrl+Z"
        onTriggered: {
            if (TimelineBridge)
                TimelineBridge.undo();

        }
    }

    Action {
        id: redoAction

        text: "やり直す"
        shortcut: "Ctrl+Shift+Z"
        onTriggered: {
            if (TimelineBridge)
                TimelineBridge.redo();

        }
    }

    Platform.MessageDialog {
        id: errorDialog

        title: "エラー"
        buttons: Platform.MessageDialog.Ok
    }

    Connections {
        function onErrorOccurred(message) {
            errorDialog.text = message;
            errorDialog.open();
        }

        target: TimelineBridge
    }

    Platform.FileDialog {
        id: saveDialog

        fileMode: Platform.FileDialog.SaveFile
        nameFilters: ["Rina Project files (*.rina)", "JSON files (*.json)"]
        defaultSuffix: "rina"
        onAccepted: {
            if (TimelineBridge)
                TimelineBridge.saveProject(file);

        }
    }

    Platform.FileDialog {
        id: loadDialog

        fileMode: Platform.FileDialog.OpenFile
        nameFilters: ["Rina Project files (*.rina)", "JSON files (*.json)"]
        onAccepted: {
            if (TimelineBridge)
                TimelineBridge.loadProject(file);

        }
    }

    Platform.FileDialog {
        id: exportDialog

        fileMode: Platform.FileDialog.SaveFile
        nameFilters: ["PNG Sequence (*.png)", "AVI Video (*.avi)"]
        onAccepted: {
            if (TimelineBridge) {
                var quality = (SettingsManager && SettingsManager.settings) ? (SettingsManager.settings.exportImageQuality || 95) : 95;
                TimelineBridge.exportMedia(file, "image_sequence", quality);
            }

        }
    }

    CompositeView {
        anchors.fill: parent
    }

    // 重要: View3D の背後に黒背景を強制
    background: Rectangle {
        color: "#000000"
    }

    menuBar: MenuBar {
        Menu {
            title: "ファイル"

            MenuItem {
                action: newAction
            }

            MenuItem {
                action: loadAction
            }
            MenuSeparator {
            }
            MenuItem {
                action: saveProjectAction
            }
            MenuItem {
                action: saveAsProjectAction
            }
            MenuSeparator {
            }
            MenuItem {
                text: "メディアのエクスポート..."
                enabled: TimelineBridge && TimelineBridge.project
                onTriggered: exportDialog.open()
            }

            MenuItem {
                text: "拡張編集AVI/BMP出力 (RGBA)"
                enabled: TimelineBridge && TimelineBridge.project
            }

            MenuSeparator {}

            MenuItem {
                text: "環境設定"
                onTriggered: {
                    if (WindowManager)
                        WindowManager.systemSettingsVisible = true;

                }
            }
            MenuSeparator {}

            MenuItem {
                text: "終了"
                action: quitAction
            }

        }

        Menu {
            title: "フィルタ"

            MenuItem {
                text: "エフェクトの追加"
                enabled: TimelineBridge && TimelineBridge.selection && TimelineBridge.selection.selectedClipId >= 0
                onTriggered: {
                    if (WindowManager)
                        WindowManager.objectSettingsVisible = true;

                }
            }

            MenuSeparator {
            }

            MenuItem {
                text: "全てのエフェクトをOFFにする"
                enabled: TimelineBridge && TimelineBridge.selection && TimelineBridge.selection.selectedClipId >= 0
                onTriggered: {
                    // TODO: Implement disableAllEffects in TimelineBridge
                    console.log("Disable all effects triggered");
                }
            }

        }

        Menu {
            title: "設定"

            MenuItem {
                text: "サイズの変更"
                onTriggered: {
                    if (WindowManager)
                        WindowManager.projectSettingsVisible = true;

                }
            }

            MenuItem {
                text: "フレームレートの変更"
                onTriggered: {
                    if (WindowManager)
                        WindowManager.projectSettingsVisible = true;

                }
            }

            MenuSeparator {
            }

            MenuItem {
                text: "環境設定"
                onTriggered: {
                    if (WindowManager)
                        WindowManager.systemSettingsVisible = true;

                }
            }

        }

        Menu {
            title: "編集"

            MenuItem {
                action: undoAction
            }

            MenuItem {
                action: redoAction
            }

            MenuSeparator {
            }

            MenuItem {
                text: "すべてのフレームを選択"
                enabled: false
            }

            MenuItem {
                text: "選択範囲のフレームを削除"
                enabled: false
            }

            MenuSeparator {
            }

            MenuItem {
                text: "再生速度の情報を変更"
                enabled: false
            }

        }

        Menu {
            title: "表示"

            Menu {
                title: "拡大表示"

                MenuItem {
                    text: "ウィンドウサイズ"
                }

                MenuItem {
                    text: "100%"
                    onTriggered: {
                    }
                }

            }

            MenuSeparator {
            }

            MenuItem {
                text: "再生ウィンドウの表示"
                onTriggered: mainWin.requestActivate()
            }

            MenuItem {
                text: "タイムラインの表示"
                onTriggered: {
                    if (WindowManager)
                        WindowManager.timelineVisible = true;

                }
            }

            MenuItem {
                text: "設定ダイアログの表示"
                onTriggered: {
                    if (WindowManager)
                        WindowManager.objectSettingsVisible = true;

                }
            }

        }

        Menu {
            title: "その他"

            MenuItem {
                text: "バージョン情報"
                onTriggered: console.log("Rina version 0.1")
            }

            MenuSeparator {
            }

            MenuItem {
                text: "未実装機能の非表示"
                checkable: true
                checked: true
            }

        }

    }

}
