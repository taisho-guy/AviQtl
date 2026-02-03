import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs
import QtQuick.Layouts 1.15
import "common" as Common

Common.RinaWindow {
    id: root

    // プロジェクトが選択されたら他のウィンドウを開く
    signal projectSelected(string projectPath, int width, int height, double fps, int totalFrames)

    width: 700
    height: 500
    title: "Rina - プロジェクトランチャー"
    Component.onCompleted: {
        // 最近使ったプロジェクトをロード
        if (SettingsManager && SettingsManager.settings) {
            var recent = SettingsManager.settings.recentProjects || [];
            recentModel.clear();
            for (var i = 0; i < Math.min(recent.length, 10); i++) {
                recentModel.append(recent[i]);
            }
            // 設定からデフォルト値を読み込む
            widthField.text = SettingsManager.settings.defaultProjectWidth || "1920";
            heightField.text = SettingsManager.settings.defaultProjectHeight || "1080";
            fpsField.text = SettingsManager.settings.defaultProjectFps || "60";
            framesField.text = SettingsManager.settings.defaultProjectFrames || "3600";
            // テンプレートに応じて更新
            if (templateCombo.currentIndex !== 4)
                // Not "カスタム"
                templateCombo.activated(templateCombo.currentIndex);

        }
    }

    ListModel {
        id: recentModel
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        // 左側: 新規プロジェクト
        ColumnLayout {
            Layout.fillHeight: true
            Layout.preferredWidth: parent.width * 0.45
            spacing: 15

            Label {
                text: "新規プロジェクト"
                font.pixelSize: 18
                font.bold: true
            }

            GroupBox {
                title: "プロジェクト設定"
                Layout.fillWidth: true

                GridLayout {
                    columns: 2
                    rowSpacing: 10
                    columnSpacing: 10
                    anchors.fill: parent

                    Label {
                        text: "テンプレート:"
                    }

                    ComboBox {
                        id: templateCombo

                        Layout.fillWidth: true
                        model: ["HD 1080p (1920x1080, 30fps)", "HD 720p (1280x720, 30fps)", "Full HD (1920x1080, 60fps)", "4K UHD (3840x2160, 30fps)", "カスタム"]
                        onActivated: (index) => {
                            switch (index) {
                            case 0:
                                widthField.text = "1920";
                                heightField.text = "1080";
                                fpsField.text = "30";
                                break;
                            case 1:
                                widthField.text = "1280";
                                heightField.text = "720";
                                fpsField.text = "30";
                                break;
                            case 2:
                                widthField.text = "1920";
                                heightField.text = "1080";
                                fpsField.text = "60";
                                break;
                            case 3:
                                widthField.text = "3840";
                                heightField.text = "2160";
                                fpsField.text = "30";
                                break;
                            default:
                                // カスタム
                                widthField.text = SettingsManager.settings.defaultProjectWidth || "1920";
                                heightField.text = SettingsManager.settings.defaultProjectHeight || "1080";
                                fpsField.text = SettingsManager.settings.defaultProjectFps || "60";
                                break;
                            }
                        }
                    }

                    Label {
                        text: "幅 (Width):"
                    }

                    TextField {
                        id: widthField

                        Layout.fillWidth: true

                        validator: IntValidator {
                            bottom: 1
                            top: 8000
                        }

                    }

                    Label {
                        text: "高さ (Height):"
                    }

                    TextField {
                        id: heightField

                        Layout.fillWidth: true

                        validator: IntValidator {
                            bottom: 1
                            top: 8000
                        }

                    }

                    Label {
                        text: "FPS:"
                    }

                    TextField {
                        id: fpsField

                        Layout.fillWidth: true

                        validator: DoubleValidator {
                            bottom: 1
                            top: 240
                        }

                    }

                    Label {
                        text: "総フレーム数:"
                    }

                    TextField {
                        id: framesField

                        Layout.fillWidth: true

                        validator: IntValidator {
                            bottom: 1
                            top: 1e+06
                        }

                    }

                }

            }

            Button {
                text: "新規プロジェクトを作成"
                highlighted: true
                Layout.fillWidth: true
                onClicked: {
                    root.projectSelected("", parseInt(widthField.text), parseInt(heightField.text), parseFloat(fpsField.text), parseInt(framesField.text));
                    root.close();
                }
            }

            Item {
                Layout.fillHeight: true
            }

        }

        // 右側: 最近使ったプロジェクト
        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 15

            Label {
                text: "最近使ったプロジェクト"
                font.pixelSize: 18
                font.bold: true
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    id: recentListView

                    model: recentModel
                    spacing: 5

                    delegate: ItemDelegate {
                        width: recentListView.width
                        height: 60
                        onClicked: {
                            root.projectSelected(model.path, model.width, model.height, model.fps, model.totalFrames);
                            root.close();
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 5

                            Label {
                                text: model.name || "無題のプロジェクト"
                                font.bold: true
                            }

                            Label {
                                text: model.path || ""
                                font.pixelSize: 10
                                color: "gray"
                            }

                            Label {
                                text: (model.width || 1920) + "x" + (model.height || 1080) + " @ " + (model.fps || 30) + "fps"
                                font.pixelSize: 10
                            }

                        }

                    }

                }

            }

            Button {
                text: "既存プロジェクトを開く..."
                Layout.fillWidth: true
                onClicked: fileDialog.open()
            }

        }

    }

    FileDialog {
        id: fileDialog

        title: "プロジェクトファイルを開く"
        nameFilters: ["Rina Project (*.rina)", "All files (*)"]
        onAccepted: {
            var width = 1920;
            var height = 1080;
            var fps = 60;
            var totalFrames = 3600;
            if (TimelineBridge) {
                var info = TimelineBridge.getProjectInfo(fileDialog.selectedFile);
                if (info.width !== undefined)
                    width = info.width;

                if (info.height !== undefined)
                    height = info.height;

                if (info.fps !== undefined)
                    fps = info.fps;

                if (info.totalFrames !== undefined)
                    totalFrames = info.totalFrames;

            }
            root.projectSelected(fileDialog.selectedFile, width, height, fps, totalFrames);
            root.close();
        }
    }

}
