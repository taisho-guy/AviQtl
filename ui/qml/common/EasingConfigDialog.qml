import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property var effectModel: null
    property string paramName: ""
    property int keyframeFrame: 0
    // Values to be returned on acceptance
    property string selectedType: "linear"
    property var bezierParams: [0.33, 0, 0.66, 1]

    title: "イージング設定: " + paramName
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    onOpened: {
        if (!effectModel)
            return ;

        const tracks = effectModel.keyframeTracks;
        const track = tracks ? tracks[paramName] : undefined;
        if (!track)
            return ;

        for (let i = 0; i < track.length; i++) {
            if (track[i].frame === keyframeFrame) {
                const kf = track[i];
                selectedType = kf.interp || "linear";
                typeCombo.currentIndex = typeCombo.model.indexOf(selectedType);
                if (selectedType === "bezier")
                    bezierParams = [kf.bz_x1 || 0.33, kf.bz_y1 || 0, kf.bz_x2 || 0.66, kf.bz_y2 || 1];

                break;
            }
        }
    }
    onAccepted: {
        if (!effectModel)
            return ;

        const kf = effectModel.keyframeTracks[paramName].find((k) => {
            return k.frame === keyframeFrame;
        });
        if (!kf)
            return ;

        let options = {
            "interp": selectedType
        };
        if (selectedType === "bezier") {
            options.bz_x1 = bezierParams[0];
            options.bz_y1 = bezierParams[1];
            options.bz_x2 = bezierParams[2];
            options.bz_y2 = bezierParams[3];
        }
        effectModel.setKeyframe(paramName, keyframeFrame, kf.value, options);
    }

    ColumnLayout {
        spacing: 15

        RowLayout {
            Label {
                text: "種類:"
            }

            ComboBox {
                id: typeCombo

                model: effectModel ? effectModel.availableEasings() : ["linear"]
                onCurrentTextChanged: root.selectedType = currentText
            }

        }

        GroupBox {
            title: "ベジェ制御点"
            enabled: root.selectedType === "bezier"

            GridLayout {
                columns: 4
                columnSpacing: 10

                Label {
                    text: "X1"
                }

                TextField {
                    id: bzX1

                    text: root.bezierParams[0].toFixed(2)
                    onEditingFinished: root.bezierParams[0] = parseFloat(text)
                }

                Label {
                    text: "Y1"
                }

                TextField {
                    id: bzY1

                    text: root.bezierParams[1].toFixed(2)
                    onEditingFinished: root.bezierParams[1] = parseFloat(text)
                }

                Label {
                    text: "X2"
                }

                TextField {
                    id: bzX2

                    text: root.bezierParams[2].toFixed(2)
                    onEditingFinished: root.bezierParams[2] = parseFloat(text)
                }

                Label {
                    text: "Y2"
                }

                TextField {
                    id: bzY2

                    text: root.bezierParams[3].toFixed(2)
                    onEditingFinished: root.bezierParams[3] = parseFloat(text)
                }

            }

        }

    }

}
