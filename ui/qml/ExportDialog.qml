import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Rina.Core 1.0

Dialog {
    id: root

    property string selectedFile: ""
    // プロジェクト設定へのショートカット
    readonly property var project: TimelineBridge ? TimelineBridge.project : null
    readonly property int pWidth: project ? project.width : 1920
    readonly property int pHeight: project ? project.height : 1080
    // ProjectServiceはfps(double)を持っているのでそっからnum/denを作る
    readonly property double pFps: project ? project.fps : 60
    readonly property int pFpsNum: Math.round(pFps * 1000)
    readonly property int pFpsDen: 1000
    // コーデック定義
    property var codecModel: [{
        "text": "H.264 (Software / libx264)",
        "value": "libx264"
    }, {
        "text": "H.264 (NVIDIA NVENC)",
        "value": "h264_nvenc"
    }, {
        "text": "H.264 (AMD AMF)",
        "value": "h264_amf"
    }, {
        "text": "H.264 (Intel QSV)",
        "value": "h264_qsv"
    }, {
        "text": "H.264 (VAAPI)",
        "value": "h264_vaapi"
    }, {
        "text": "HEVC (Software / libx265)",
        "value": "libx265"
    }, {
        "text": "HEVC (NVIDIA NVENC)",
        "value": "hevc_nvenc"
    }, {
        "text": "HEVC (AMD AMF)",
        "value": "hevc_amf"
    }, {
        "text": "HEVC (Intel QSV)",
        "value": "hevc_qsv"
    }, {
        "text": "HEVC (VAAPI)",
        "value": "hevc_vaapi"
    }, {
        "text": "AV1 (Software / libaom)",
        "value": "libaom-av1"
    }, {
        "text": "AV1 (NVIDIA NVENC)",
        "value": "av1_nvenc"
    }, {
        "text": "AV1 (AMD AMF)",
        "value": "av1_amf"
    }, {
        "text": "AV1 (Intel QSV)",
        "value": "av1_qsv"
    }, {
        "text": "AV1 (VAAPI)",
        "value": "av1_vaapi"
    }]

    title: "メディアの書き出し"
    modal: true
    width: 600
    height: 400
    standardButtons: Dialog.Ok | Dialog.Cancel
    onAccepted: {
        if (filePathField.text === "")
            return ;

        var codec = codecModel[codecCombo.currentIndex].value;
        var bitrateVal = bitrateSpin.value * 1e+06;
        console.log("Starting Export: " + root.pWidth + "x" + root.pHeight + " @ " + codec);
        // C++側の処理を開始
        encoder.open({
            "width": root.pWidth,
            "height": root.pHeight,
            "fps_num": root.pFpsNum,
            "fps_den": root.pFpsDen,
            "bitrate": bitrateVal,
            "codecName": codec,
            "outputUrl": filePathField.text
        });
        // 先に閉じる
        root.close();
        // 少し待ってから開始 (閉じるアニメーションの完了待ち)
        var timer = Qt.createQmlObject("import QtQuick 2.0; Timer { interval: 200; repeat: false; running: true; }", root);
        timer.triggered.connect(function() {
            if (TimelineBridge)
                TimelineBridge.exportVideoHW(encoder);

        });
    }

    // C++のエンコーダーインスタンス
    VideoEncoder {
        id: encoder
    }

    // ファイル保存ダイアログ
    FileDialog {
        id: fileDialog

        title: "保存先を指定"
        fileMode: FileDialog.SaveFile
        nameFilters: ["MP4 Video (*.mp4)", "MKV Video (*.mkv)", "All files (*)"]
        onAccepted: {
            root.selectedFile = fileDialog.selectedFile;
            // file:/// prefixを除去 (Linux/Windows対応)
            var path = root.selectedFile.toString();
            if (Qt.platform.os === "windows")
                path = path.replace(/^(file:\/{3})/, "");
            else
                path = path.replace(/^(file:\/\/)/, "");
            filePathField.text = path;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        RowLayout {
            TextField {
                id: filePathField

                Layout.fillWidth: true
                placeholderText: "保存先ファイルパス..."
                text: root.selectedFile
            }

            Button {
                text: "参照..."
                onClicked: fileDialog.open()
            }

        }

        // エンコード設定情報
        GroupBox {
            title: "出力設定 (プロジェクト設定同期)"
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                rowSpacing: 10
                columnSpacing: 20

                Label {
                    text: "解像度:"
                }

                Label {
                    text: root.pWidth + " x " + root.pHeight
                    font.bold: true
                }

                Label {
                    text: "フレームレート:"
                }

                Label {
                    text: root.pFps.toFixed(2) + " fps"
                    font.bold: true
                }

                Label {
                    text: "コーデック:"
                }

                ComboBox {
                    id: codecCombo

                    model: root.codecModel.map((x) => {
                        return x.text;
                    })
                    currentIndex: 0
                }

                Label {
                    text: "品質 (Bitrate):"
                }

                SpinBox {
                    id: bitrateSpin

                    from: 1
                    to: 100
                    value: 15 // Mbps
                    editable: true
                    stepSize: 1
                    // suffixプロパティの代替実装
                    textFromValue: function(value, locale) {
                        return value + " Mbps";
                    }
                    valueFromText: function(text, locale) {
                        return Number.fromLocaleString(locale, text.replace(" Mbps", ""));
                    }
                }

            }

        }

        Item {
            Layout.fillHeight: true
        }

    }

}
