import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "common" as Common

Common.RinaWindow {
    id: root

    width: 450
    height: 200
    title: "プロジェクト設定"
    // ウィンドウが表示されたときに現在の値をUIに反映
    onVisibleChanged: {
        if (visible && TimelineBridge && TimelineBridge.project) {
            widthField.text = TimelineBridge.project.width;
            heightField.text = TimelineBridge.project.height;
            fpsField.text = TimelineBridge.project.fps;
            sampleRateField.text = TimelineBridge.project.sampleRate;
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 15

        GridLayout {
            columns: 2
            rowSpacing: 10
            columnSpacing: 10

            Label {
                text: "幅:"
            }

            TextField {
                id: widthField

                selectByMouse: true

                validator: IntValidator {
                    bottom: 1
                    top: 8000
                }

            }

            Label {
                text: "高さ:"
            }

            TextField {
                id: heightField

                selectByMouse: true

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

                selectByMouse: true

                validator: DoubleValidator {
                    bottom: 1
                    top: 240
                }

            }

            Label {
                text: "サンプリングレート:"
            }

            TextField {
                id: sampleRateField

                selectByMouse: true

                validator: IntValidator {
                    bottom: 8000
                    top: 192000
                }

            }

        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter

            Button {
                text: "適用"
                highlighted: true
                onClicked: {
                    if (TimelineBridge && TimelineBridge.project) {
                        TimelineBridge.project.width = parseInt(widthField.text);
                        TimelineBridge.project.height = parseInt(heightField.text);
                        TimelineBridge.project.fps = parseFloat(fpsField.text);
                        TimelineBridge.project.sampleRate = parseInt(sampleRateField.text);
                        root.hide();
                    }
                }
            }

            Button {
                text: "キャンセル"
                onClicked: root.hide()
            }

        }

    }

}
