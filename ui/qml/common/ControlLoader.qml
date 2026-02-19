import "." as Common // 同じディレクトリ内のコンポーネント(ParamControl)を参照
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs
import QtQuick.Layouts 1.15

Loader {
    // ===============================================
    //               Components
    // ===============================================
    // アニメーション可能な数値 (ParamControl)
    // 単一の整数値 (シーンIDなど、イージング不可)
    // 色選択
    // 真偽値 (チェックボックス)
    // ファイルパス (ファイル選択ボタン付き)
    // 文字列
    // 列挙型 (コンボボックス)
    // コンボボックス (動的ソース)
    // ヘッダー (区切り線)

    id: controlLoader

    // === 入力プロパティ ===
    // 定義オブジェクト: { type: "float", name: "opacity", label: "不透明度", min: 0, max: 1, ... }
    property var definition: ({
    })
    // 現在の値 (バインディング可能)
    property var value: null

    // === 出力シグナル ===
    signal valueModified(var newValue)
    // 追加: ParamControlの中央ボタンクリック（イージング設定など）を親に伝えるシグナル
    signal paramButtonClicked()

    // === コンポーネント選択ロジック ===
    sourceComponent: {
        if (!definition)
            return unknownComponent;

        switch (definition.type) {
        case "float":
        case "number":
        case "slider":
        case "spinner":
            return floatComponent;
        case "int":
        case "integer":
        case "scene_id":
            return intComponent;
        case "color":
            return colorComponent;
        case "bool":
        case "boolean":
            return boolComponent;
        case "path":
        case "file":
            return pathComponent;
        case "string":
        case "text":
            return stringComponent;
        case "enum":
            return enumComponent;
        case "combo":
            return comboComponent;
        case "font":
            return fontComponent;
        case "header":
            return headerComponent;
        default:
            // デフォルト
            return unknownComponent;
        }
    }

    // --- 1. Float / Number (ParamControl) ---
    // アニメーション、イージング、中間点に対応した高機能スライダー
    Component {
        id: floatComponent

        Common.ParamControl {
            paramName: controlLoader.definition.label || controlLoader.definition.param || controlLoader.definition.name || "Param"
            // 値の初期化 (null/undefinedチェック)
            startValue: (controlLoader.value !== undefined && controlLoader.value !== null) ? Number(controlLoader.value) : (controlLoader.definition.default || 0)
            // ※ 現状のControlLoaderは単一値(value)しか受け取っていないため、endValueはstartValueと同じにするか、
            //    将来的にControlLoader自体がstart/endを受け取るように拡張する必要がある。
            //    ここでは「現在値」を表示する簡易実装とする。
            endValue: startValue
            isRangeMode: false
            minValue: controlLoader.definition.min !== undefined ? controlLoader.definition.min : -100000
            maxValue: controlLoader.definition.max !== undefined ? controlLoader.definition.max : 100000
            decimals: 2
            // ControlLoaderは単一の値を返すラッパーとして振る舞う
            onStartValueModified: function(val) {
                controlLoader.valueModified(val);
            }
            onEndValueModified: function(val) {
                // 将来的にはここで終了値を別のプロパティとして書き戻す
                // 現状はstartと同じ扱いにするか、無視する
                controlLoader.valueModified(val);
            }
            // 追加: ボタンクリックイベントの転送
            onParamButtonClicked: {
                controlLoader.paramButtonClicked();
            }
        }

    }

    // --- 2. Int / Scene ID ---
    // アニメーションしない単純な整数入力
    Component {
        id: intComponent

        RowLayout {
            spacing: 8

            Label {
                text: controlLoader.definition.label || controlLoader.definition.name
                color: "white"
                Layout.preferredWidth: 80
                elide: Text.ElideRight
            }

            TextField {
                text: String(controlLoader.value !== undefined ? controlLoader.value : (controlLoader.definition.default || 0))
                Layout.preferredWidth: 80
                horizontalAlignment: TextInput.AlignRight
                selectByMouse: true
                onEditingFinished: {
                    var val = parseInt(text);
                    if (!isNaN(val))
                        controlLoader.valueModified(val);

                }

                validator: IntValidator {
                    bottom: controlLoader.definition.min !== undefined ? controlLoader.definition.min : -2.14748e+09
                    top: controlLoader.definition.max !== undefined ? controlLoader.definition.max : 2.14748e+09
                }

            }

            // 単位などのラベルがあれば表示
            Label {
                text: controlLoader.definition.unit || ""
                color: "#888"
                visible: text.length > 0
            }

            // Spacer
            Item {
                Layout.fillWidth: true
            }

        }

    }

    // --- 3. Color ---
    Component {
        id: colorComponent

        RowLayout {
            spacing: 8

            ColorDialog {
                id: colorDialog

                selectedColor: controlLoader.value || "#FFFFFF"
                onAccepted: controlLoader.valueModified(selectedColor.toString())
            }

            Label {
                text: controlLoader.definition.label || controlLoader.definition.name
                color: "white"
                Layout.preferredWidth: 80
            }

            Rectangle {
                width: 60
                height: 24
                color: controlLoader.value || "#FFFFFF"
                border.color: "white"
                border.width: 1
                radius: 2

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        colorDialog.open();
                    }
                }

            }

            // HEX値の直接入力
            TextField {
                text: controlLoader.value || "#FFFFFF"
                Layout.preferredWidth: 80
                selectByMouse: true
                onEditingFinished: {
                    controlLoader.valueModified(text);
                }
            }

        }

    }

    // --- 4. Bool (Checkbox) ---
    Component {
        id: boolComponent

        RowLayout {
            spacing: 8

            Label {
                text: controlLoader.definition.label || controlLoader.definition.name
                color: "white"
                Layout.preferredWidth: 80
            }

            CheckBox {
                checked: controlLoader.value === true
                text: checked ? "On" : "Off"
                onCheckedChanged: {
                    controlLoader.valueModified(checked);
                }
            }

        }

    }

    // --- 5. Path / File ---
    Component {
        id: pathComponent

        RowLayout {
            spacing: 8

            FileDialog {
                id: fileDialog

                title: controlLoader.definition.label || "ファイルを選択"
                currentFile: (controlLoader.value && controlLoader.value.toString() !== "") ? (controlLoader.value.toString().startsWith("file://") ? controlLoader.value : "file://" + controlLoader.value) : ""
                nameFilters: {
                    var f = controlLoader.definition.filter;
                    if (Array.isArray(f))
                        return f;

                    return f ? [f, "All files (*)"] : ["All files (*)"];
                }
                onAccepted: {
                    var path = selectedFile.toString();
                    if (Qt.platform.os === "windows")
                        path = path.replace(/^(file:\/{3})/, "");
                    else
                        path = path.replace(/^(file:\/\/)/, "");
                    controlLoader.valueModified(path);
                }
            }

            Label {
                text: controlLoader.definition.label || controlLoader.definition.name
                color: "white"
                Layout.preferredWidth: 80
            }

            TextField {
                text: String(controlLoader.value || "")
                Layout.fillWidth: true
                selectByMouse: true
                onEditingFinished: {
                    controlLoader.valueModified(text);
                }
            }

            Button {
                text: "..."
                Layout.preferredWidth: 30
                onClicked: {
                    fileDialog.open();
                }
            }

        }

    }

    // --- 6. String / Text ---
    Component {
        id: stringComponent

        RowLayout {
            spacing: 8

            Label {
                text: controlLoader.definition.label || controlLoader.definition.name
                color: "white"
                Layout.preferredWidth: 80
            }

            TextField {
                text: String(controlLoader.value || "")
                Layout.fillWidth: true
                selectByMouse: true
                onEditingFinished: {
                    controlLoader.valueModified(text);
                }
            }

        }

    }

    // --- 7. Enum (ComboBox) ---
    Component {
        id: enumComponent

        RowLayout {
            spacing: 8

            Label {
                text: controlLoader.definition.label || controlLoader.definition.name
                color: "white"
                Layout.preferredWidth: 80
            }

            ComboBox {
                model: controlLoader.definition.options || []
                Layout.fillWidth: true
                // 現在の値(文字列)に対応するインデックスを探す
                currentIndex: {
                    var idx = model.indexOf(controlLoader.value);
                    return idx >= 0 ? idx : 0;
                }
                onActivated: {
                    // ユーザー操作時のみ発火
                    controlLoader.valueModified(currentText);
                }
            }

        }

    }

    // --- 9. Combo (Dynamic Source) ---
    Component {
        id: comboComponent

        RowLayout {
            spacing: 8

            Label {
                text: controlLoader.definition.label || controlLoader.definition.name
                color: "white"
                Layout.preferredWidth: 80
            }

            ComboBox {
                id: combo

                // ソースの解決 (TimelineBridge等)
                property var sourceObj: {
                    if (controlLoader.definition.source === "TimelineBridge")
                        return TimelineBridge;

                    return null;
                }

                Layout.fillWidth: true
                model: sourceObj ? sourceObj[controlLoader.definition.sourceProperty] : []
                textRole: controlLoader.definition.textRole || "name"
                valueRole: controlLoader.definition.valueRole || "id"
                // 現在値の反映
                currentIndex: {
                    var val = controlLoader.value;
                    if (!model)
                        return -1;

                    for (var i = 0; i < count; i++) {
                        var item = model[i];
                        if (item && item[valueRole] == val)
                            return i;

                    }
                    return -1;
                }
                onActivated: (index) => {
                    if (index >= 0 && model && model[index])
                        controlLoader.valueModified(model[index][valueRole]);

                }
            }

        }

    }

    // --- font: FontDialog ベースフォントピッカー ─────────────────────
    Component {
        id: fontComponent

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            FontDialog {
                id: fontPickerDialog

                title: controlLoader.definition.label || "フォントを選択"
                // 現在値をダイアログの初期選択に反映
                currentFont.family: controlLoader.value || ""
                onAccepted: controlLoader.valueModified(selectedFont.family)
            }

            Label {
                text: controlLoader.definition.label || controlLoader.definition.name
                color: "white"
                Layout.preferredWidth: 80
                elide: Text.ElideRight
            }

            // 現在のフォント名を表示 + クリックでダイアログを開く
            Button {
                Layout.fillWidth: true
                text: controlLoader.value ? controlLoader.value : "デフォルト"
                // ボタン自体をそのフォントでプレビュー表示
                font.family: controlLoader.value || ""
                font.pixelSize: 13
                onClicked: fontPickerDialog.open()
            }

        }

    }

    // --- 8. Header ---
    Component {
        id: headerComponent

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.bottomMargin: 4

            Label {
                text: controlLoader.definition.label || "Settings"
                font.bold: true
                font.pixelSize: 13
                color: "#cccccc"
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#444444"
            }

        }

    }

    // --- Fallback ---
    Component {
        id: unknownComponent

        Text {
            text: "Unknown Type: " + (controlLoader.definition ? controlLoader.definition.type : "null")
            color: "red"
            font.pixelSize: 10
        }

    }

}
