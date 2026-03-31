import "../common" as Common
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Common.RinaWindow {
    id: root

    // 設定対象のTimelineView
    property var targetView: null

    function open(view) {
        targetView = view;
        if (view && view.gridSettings) {
            var s = view.gridSettings;
            if (s.mode === "BPM")
                modeCombo.currentIndex = 1;
            else if (s.mode === "Frame")
                modeCombo.currentIndex = 2;
            else
                modeCombo.currentIndex = 0; // Auto
            bpmField.text = s.bpm || 120;
            offsetField.text = s.offset || 0;
            intervalField.text = s.interval || 10;
            subdivisionField.text = s.subdivision || 4;
        }
        root.show();
        root.raise();
    }

    function apply() {
        if (!targetView)
            return ;

        var modeKey = "Auto";
        if (modeCombo.currentIndex === 1)
            modeKey = "BPM";

        if (modeCombo.currentIndex === 2)
            modeKey = "Frame";

        // 新しい設定オブジェクトを代入することで、TimelineView側のonGridSettingsChangedを発火させる
        targetView.gridSettings = {
            "mode": modeKey,
            "bpm": parseFloat(bpmField.text) || 120,
            "offset": parseFloat(offsetField.text) || 0,
            "interval": parseInt(intervalField.text) || 10,
            "subdivision": parseInt(subdivisionField.text) || 4
        };
    }

    title: qsTr("グリッド設定")
    width: 320
    height: 300

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 10

        RowLayout {
            Label {
                text: qsTr("モード:")
            }

            ComboBox {
                id: modeCombo

                Layout.fillWidth: true
                model: [qsTr("自動 (秒/フレーム)"), qsTr("BPM (音楽)"), qsTr("フレーム数固定")]
                onCurrentIndexChanged: apply()
            }

        }

        // BPM設定
        GroupBox {
            title: qsTr("BPM設定")
            visible: modeCombo.currentIndex === 1
            Layout.fillWidth: true

            GridLayout {
                columns: 2

                Label {
                    text: qsTr("BPM:")
                }

                TextField {
                    id: bpmField

                    text: "120"
                    selectByMouse: true
                    onEditingFinished: apply()
                }

                Label {
                    text: qsTr("拍子 (分割数):")
                }

                TextField {
                    id: subdivisionField

                    text: "4"
                    selectByMouse: true
                    onEditingFinished: apply()
                }

                Label {
                    text: qsTr("オフセット (秒):")
                }

                TextField {
                    id: offsetField

                    text: "0.0"
                    selectByMouse: true
                    onEditingFinished: apply()
                }

            }

        }

        // フレーム設定
        GroupBox {
            title: qsTr("フレーム設定")
            visible: modeCombo.currentIndex === 2
            Layout.fillWidth: true

            GridLayout {
                columns: 2

                Label {
                    text: qsTr("間隔 (フレーム):")
                }

                TextField {
                    id: intervalField

                    text: "10"
                    selectByMouse: true
                    onEditingFinished: apply()
                }

            }

        }

        // Spacer
        Item {
            Layout.fillHeight: true
        }

        Button {
            text: qsTr("閉じる")
            Layout.alignment: Qt.AlignRight
            onClicked: root.close()
        }

    }

}
