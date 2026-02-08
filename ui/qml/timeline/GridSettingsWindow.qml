import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../common" as Common

Common.RinaWindow {
    id: root
    title: "グリッド設定"
    width: 320
    height: 300

    // 設定対象のTimelineView
    property var targetView: null

    function open(view) {
        targetView = view;
        if (view && view.gridSettings) {
            var s = view.gridSettings;
            if (s.mode === "BPM") modeCombo.currentIndex = 1;
            else if (s.mode === "Frame") modeCombo.currentIndex = 2;
            else modeCombo.currentIndex = 0; // Auto

            bpmField.text = s.bpm || 120;
            offsetField.text = s.offset || 0.0;
            intervalField.text = s.interval || 10;
            subdivisionField.text = s.subdivision || 4;
        }
        root.show();
        root.raise();
    }

    function apply() {
        if (!targetView) return;
        
        var modeKey = "Auto";
        if (modeCombo.currentIndex === 1) modeKey = "BPM";
        if (modeCombo.currentIndex === 2) modeKey = "Frame";

        // 新しい設定オブジェクトを代入することで、TimelineView側のonGridSettingsChangedを発火させる
        targetView.gridSettings = {
            mode: modeKey,
            bpm: parseFloat(bpmField.text) || 120,
            offset: parseFloat(offsetField.text) || 0.0,
            interval: parseInt(intervalField.text) || 10,
            subdivision: parseInt(subdivisionField.text) || 4
        };
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 10

        RowLayout {
            Label { text: "モード:" }
            ComboBox {
                id: modeCombo
                Layout.fillWidth: true
                model: ["自動 (秒/フレーム)", "BPM (音楽)", "フレーム数固定"]
                onCurrentIndexChanged: apply()
            }
        }

        // BPM設定
        GroupBox {
            title: "BPM設定"
            visible: modeCombo.currentIndex === 1
            Layout.fillWidth: true
            GridLayout {
                columns: 2
                Label { text: "BPM:" }
                TextField { id: bpmField; text: "120"; selectByMouse: true; onEditingFinished: apply() }
                Label { text: "拍子 (分割数):" }
                TextField { id: subdivisionField; text: "4"; selectByMouse: true; onEditingFinished: apply() }
                Label { text: "オフセット (秒):" }
                TextField { id: offsetField; text: "0.0"; selectByMouse: true; onEditingFinished: apply() }
            }
        }

        // フレーム設定
        GroupBox {
            title: "フレーム設定"
            visible: modeCombo.currentIndex === 2
            Layout.fillWidth: true
            GridLayout {
                columns: 2
                Label { text: "間隔 (Frames):" }
                TextField { id: intervalField; text: "10"; selectByMouse: true; onEditingFinished: apply() }
            }
        }

        Item { Layout.fillHeight: true } // Spacer

        Button {
            text: "閉じる"
            Layout.alignment: Qt.AlignRight
            onClicked: root.close()
        }
    }
}
