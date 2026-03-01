import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "common" as Common

Common.RinaWindow {
    id: root

    property int targetSceneId: -1
    property int currentFrames: 0 // 既存のフレーム数を保持するためのプロパティ

    function openForScene(sceneId, name, w, h, fps, frames) {
        targetSceneId = sceneId;
        nameField.text = name;
        widthField.value = w;
        heightField.value = h;
        fpsField.value = Math.round(fps * 100);
        currentFrames = frames;
        root.show();
        root.raise();
        root.requestActivate();
    }

    title: "シーン設定"
    width: 450
    height: 300

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 10

        RowLayout {
            Label {
                text: "シーン名:"
                Layout.preferredWidth: 100
            }

            TextField {
                id: nameField

                Layout.fillWidth: true
                selectByMouse: true
            }

        }

        RowLayout {
            Label {
                text: "幅:"
                Layout.preferredWidth: 100
            }

            SpinBox {
                id: widthField

                from: 1
                to: SettingsManager ? SettingsManager.value("sceneWidthMax", 8000) : 8000
                editable: true
                Layout.fillWidth: true
            }

        }

        RowLayout {
            Label {
                text: "高さ:"
                Layout.preferredWidth: 100
            }

            SpinBox {
                id: heightField

                from: 1
                to: 8000
                editable: true
                Layout.fillWidth: true
            }

        }

        RowLayout {
            Label {
                text: "FPS:"
                Layout.preferredWidth: 100
            }

            SpinBox {
                id: fpsField

                property real realValue: value / 100

                from: SettingsManager ? SettingsManager.value("sceneFramesMin", 100) : 100
                to: SettingsManager ? SettingsManager.value("sceneFramesMax", 24000) : 24000
                stepSize: SettingsManager ? SettingsManager.value("sceneFramesStep", 100) : 100
                editable: true
                Layout.fillWidth: true
                textFromValue: function(value, locale) {
                    return (value / 100).toFixed(2);
                }
                valueFromText: function(text, locale) {
                    return Number(text) * 100;
                }
            }

        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 10

            Button {
                text: "キャンセル"
                onClicked: root.hide()
            }

            Button {
                text: "OK"
                highlighted: true
                onClicked: {
                    if (targetSceneId !== -1 && TimelineBridge)
                        TimelineBridge.updateSceneSettings(targetSceneId, nameField.text, widthField.value, heightField.value, fpsField.realValue, currentFrames);

                    root.hide();
                }
            }

        }

    }

}
