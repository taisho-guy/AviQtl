import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property var effectModel: null
    property string paramName: ""
    property int keyframeFrame: 0
    property string selectedType: "linear"
    property var bezierParams: [0.33, 0, 0.66, 1]

    function requestPreview() {
        if (previewCanvas)
            previewCanvas.requestPaint();

    }

    title: "イージング設定: " + paramName + " [" + selectedType + "]"
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    onSelectedTypeChanged: requestPreview()
    onBezierParamsChanged: requestPreview()
    onOpened: {
        if (!effectModel)
            return ;

        typeCombo.model = effectModel.availableEasings();
        const tracks = effectModel.keyframeTracks;
        const track = tracks ? tracks[paramName] : undefined;
        if (!track)
            return ;

        for (let i = 0; i < track.length; i++) {
            if (track[i].frame === keyframeFrame) {
                const kf = track[i];
                selectedType = kf.interp || "linear";
                var idx = typeCombo.model.indexOf(selectedType);
                typeCombo.currentIndex = idx >= 0 ? idx : 0;
                if (selectedType === "bezier")
                    bezierParams = [kf.bzx1 !== undefined ? kf.bzx1 : 0.33, kf.bzy1 !== undefined ? kf.bzy1 : 0, kf.bzx2 !== undefined ? kf.bzx2 : 0.66, kf.bzy2 !== undefined ? kf.bzy2 : 1];

                break;
            }
        }
        requestPreview();
    }
    onAccepted: {
        if (!effectModel)
            return ;

        const kf = effectModel.keyframeTracks[paramName].find(function(k) {
            return k.frame === keyframeFrame;
        });
        if (!kf)
            return ;

        let options = {
            "interp": selectedType
        };
        if (selectedType === "bezier") {
            options.bzx1 = bezierParams[0];
            options.bzy1 = bezierParams[1];
            options.bzx2 = bezierParams[2];
            options.bzy2 = bezierParams[3];
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

                model: ["linear"]
                onActivated: function(idx) {
                    root.selectedType = typeCombo.currentText;
                }
            }

        }

        GroupBox {
            title: "カーブプレビュー"
            Layout.fillWidth: true

            Canvas {
                id: previewCanvas

                Layout.fillWidth: true
                Layout.preferredHeight: 160
                implicitWidth: 320
                implicitHeight: 160
                onPaint: {
                    var ctx = getContext("2d");
                    var w = width;
                    var h = height;
                    ctx.clearRect(0, 0, w, h);
                    ctx.fillStyle = "#222";
                    ctx.fillRect(0, 0, w, h);
                    ctx.strokeStyle = "#444";
                    ctx.lineWidth = 0.5;
                    for (var i = 1; i < 4; i++) {
                        var gx = w * (i / 4);
                        var gy = h * (i / 4);
                        ctx.beginPath();
                        ctx.moveTo(gx, 0);
                        ctx.lineTo(gx, h);
                        ctx.stroke();
                        ctx.beginPath();
                        ctx.moveTo(0, h - gy);
                        ctx.lineTo(w, h - gy);
                        ctx.stroke();
                    }
                    ctx.strokeStyle = "#555";
                    ctx.lineWidth = 1;
                    ctx.beginPath();
                    ctx.moveTo(0, h - 1);
                    ctx.lineTo(w, 1);
                    ctx.stroke();
                    ctx.strokeStyle = "#48f";
                    ctx.lineWidth = 2;
                    var bz = root.bezierParams;
                    var steps = 64;
                    ctx.beginPath();
                    for (var s = 0; s <= steps; s++) {
                        var t = s / steps;
                        var y = evalEasing(t);
                        var px = t * w;
                        var py = h - Math.max(0, Math.min(1, y)) * h;
                        if (s === 0)
                            ctx.moveTo(px, py);
                        else
                            ctx.lineTo(px, py);
                    }
                    ctx.stroke();
                }

                // evalEasing を onPaint 内に定義して bz をキャプチャ
                function evalEasing(t) {
                    // 正規表現ですべてのアンダースコアを除去
                    var type = root.selectedType.toLowerCase().replace(/_/g, "");
                    if (type === "easein") {
                        return t * t;
                    } else if (type === "easeout") {
                        return t * (2 - t);
                    } else if (type === "easeinout") {
                        return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
                    } else if (type === "bezier") {
                        var x1 = (bz && bz[0] != null) ? bz[0] : 0.33;
                        var y1 = (bz && bz[1] != null) ? bz[1] : 0;
                        var x2 = (bz && bz[2] != null) ? bz[2] : 0.66;
                        var y2 = (bz && bz[3] != null) ? bz[3] : 1;
                        var T = t;
                        for (var k = 0; k < 8; k++) {
                            var u = 1 - T;
                            var x = 3 * u * u * T * x1 + 3 * u * T * T * x2 + T * T * T;
                            var err = x - t;
                            if (Math.abs(err) < 0.0001)
                                break;

                            var dxdT = 3 * u * u * x1 + 6 * u * T * (x2 - x1) + 3 * T * T * (1 - x2);
                            if (Math.abs(dxdT) < 1e-06)
                                break;

                            T -= err / dxdT;
                        }
                        if (T < 0)
                            T = 0;

                        if (T > 1)
                            T = 1;

                        var u2 = 1 - T;
                        return 3 * u2 * u2 * T * y1 + 3 * u2 * T * T * y2 + T * T * T;
                    } else {
                        return t;
                    }
                }

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
                    text: root.bezierParams[0].toFixed(2)
                    onEditingFinished: root.bezierParams = [parseFloat(text), root.bezierParams[1], root.bezierParams[2], root.bezierParams[3]]
                }

                Label {
                    text: "Y1"
                }

                TextField {
                    text: root.bezierParams[1].toFixed(2)
                    onEditingFinished: root.bezierParams = [root.bezierParams[0], parseFloat(text), root.bezierParams[2], root.bezierParams[3]]
                }

                Label {
                    text: "X2"
                }

                TextField {
                    text: root.bezierParams[2].toFixed(2)
                    onEditingFinished: root.bezierParams = [root.bezierParams[0], root.bezierParams[1], parseFloat(text), root.bezierParams[3]]
                }

                Label {
                    text: "Y2"
                }

                TextField {
                    text: root.bezierParams[3].toFixed(2)
                    onEditingFinished: root.bezierParams = [root.bezierParams[0], root.bezierParams[1], root.bezierParams[2], parseFloat(text)]
                }

            }

        }

    }

}
