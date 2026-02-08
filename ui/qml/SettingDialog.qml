import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "common" as Common

Common.RinaWindow {
    id: root

    property int targetClipId: (TimelineBridge && TimelineBridge.selection) ? TimelineBridge.selection.selectedClipId : -1
    property var effectsModel: []
    property bool inputting: false // 入力中フラグ（reloadループ防止用）

    function reload() {
        if (!TimelineBridge || !TimelineBridge.selection)
            return ;

        var id = TimelineBridge.selection.selectedClipId;
        targetClipId = id;
        if (id >= 0)
            effectsModel = TimelineBridge.getClipEffectsModel(id);
        else
            effectsModel = [];
    }

    width: 350
    height: 500
    title: "設定ダイアログ"
    color: palette.window
    visible: true
    x: 500
    y: 200
    Component.onCompleted: reload()

    Component {
        id: easingDialogComponent

        Common.EasingConfigDialog {
        }

    }

    // 選択変更やデータ更新を監視してモデルをリロード
    Connections {
        function onSelectedClipIdChanged() {
            reload();
        }

        // パラメータ変更時の反映用（入力中はリロードしない）
        function onSelectedClipDataChanged() {
            if (!inputting)
                reload();

        }

        target: TimelineBridge ? TimelineBridge.selection : null
    }

    Connections {
        function onClipEffectsChanged(clipId) {
            if (clipId === targetClipId)
                reload();

        }

        target: TimelineBridge
    }

    // UI定義を正規化してリストとして取得するヘルパー
    function getUiModel(effectModel) {
        var ui = effectModel.uiDefinition;
        if (ui) {
            // 1. 標準形式: { controls: [...] }
            if (ui.controls && typeof ui.controls.length === 'number') return ui.controls;
            
            // 2. マップ形式 (後方互換): { "param": { ... } }
            var keys = Object.keys(ui);
            if (keys.length > 0 && keys.indexOf("controls") === -1 && keys.indexOf("group") === -1) {
                var list = [];
                for (var i=0; i<keys.length; i++) {
                    var def = ui[keys[i]];
                    if (typeof def === 'object') {
                        def.param = keys[i];
                        list.push(def);
                    }
                }
                return list;
            }
        }
        // 3. フォールバック: paramsから自動生成
        var params = effectModel.params;
        var pKeys = Object.keys(params).sort();
        var autoList = [];
        for (var j=0; j<pKeys.length; j++) {
            autoList.push({ 
                type: typeof params[pKeys[j]] === 'number' ? 'float' : 'string', 
                param: pKeys[j], 
                label: pKeys[j] 
            });
        }
        return autoList;
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: parent.width
        clip: true

        ColumnLayout {
            width: root.width
            spacing: 1

            Repeater {
                model: effectsModel

                delegate: ColumnLayout {
                    id: effectRoot

                    // 親デリゲートでデータとインデックスを公開
                    property int effectIndex: index
                    property var effectModel: modelData
                    // EffectModel を一次ソースとして常に参照する
                    property var currentParams: effectModel ? effectModel.params : ({
                    })
                    
                    // UI定義の有無を判定するプロパティをここに移動
                    property var uiDef: effectModel ? effectModel.uiDefinition : null
                    property bool hasExplicitUi: {
                        if (!uiDef) return false;
                        // 配列形式のチェック
                        if (typeof uiDef.length === 'number' && uiDef.length > 0) return true;
                        // オブジェクト形式(controlsプロパティ)のチェック
                        if (uiDef.controls && typeof uiDef.controls.length === 'number' && uiDef.controls.length > 0) return true;
                        return false;
                    }

                    width: root.width
                    spacing: 0

                    // エフェクトヘッダー
                    Rectangle {
                        Layout.fillWidth: true
                        height: 24
                        color: palette.midlight

                        Label {
                            text: modelData.name
                            color: palette.text
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                        }

                        // 削除ボタン (Transform以外)
                        Button {
                            text: "×"
                            visible: modelData.id !== "transform" && modelData.id !== "rect" && modelData.id !== "text"
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            onClicked: TimelineBridge.removeEffect(targetClipId, effectRoot.effectIndex)
                        }

                    }

                    // パラメータリスト
                    // uiDefinition がある場合はそれを優先、なければ従来のパラメータループ
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        // === 方式A: uiDefinition ベース (新方式) ===
                        Repeater {
                            model: effectRoot.hasExplicitUi ? getUiModel(effectModel) : []

                            delegate: Common.ControlLoader {
                                Layout.fillWidth: true
                                Layout.margins: 4
                                definition: modelData
                                value: effectRoot.currentParams[modelData.param || modelData.name]
                                
                                onValueModified: function(val) {
                                    root.inputting = true;
                                    TimelineBridge.updateClipEffectParam(targetClipId, effectRoot.effectIndex, modelData.param || modelData.name, val);
                                    root.inputting = false;
                                }
                            }
                        }

                        // === 方式B: レガシー (既存のパラメータキーベース) ===
                        Repeater {
                            model: effectRoot.hasExplicitUi ? [] : Object.keys(effectRoot.currentParams).sort()

                            delegate: ColumnLayout {
                                property string key: modelData
                                property var effectModel: effectRoot.effectModel
                                property int effIdx: effectRoot.effectIndex
                                property var effVal: effectRoot.effectModel ? effectRoot.effectModel.params[key] : undefined
                                property bool isNumber: typeof effVal === 'number'
                                
                                // キーフレーム関連のロジック
                                property int curRelFrame: (TimelineBridge && TimelineBridge.transport) ? (TimelineBridge.transport.currentFrame - TimelineBridge.clipStartFrame) : 0
                                property int clipDur: TimelineBridge ? TimelineBridge.clipDurationFrames : 100
                                property var tracks: effectModel.keyframeTracks
                                property var track: tracks ? tracks[key] : undefined
                                property var kfs: track || []
                                property bool hasKeyframes: kfs.length > 0
                                property var interval: findInterval(kfs, curRelFrame, clipDur)
                                property int startFrame: interval.start
                                property int endFrame: interval.end
                                property var startVal: isNumber ? (tracks, effVal, effectModel.evaluatedParam(key, startFrame)) : effVal
                                property var endVal: isNumber ? (tracks, effVal, effectModel.evaluatedParam(key, endFrame)) : effVal
                                property string interpType: hasKeyframes ? getInterpAt(startFrame) : "constant"
                                property bool isMoving: isNumber && (hasKeyframes || interpType !== "constant")

                                function findInterval(kfs, cur, totalDur) {
                                    let s = 0, e = totalDur;
                                    if (!kfs || kfs.length === 0) return {"start": s, "end": e};
                                    let foundStart = false;
                                    for (let i = kfs.length - 1; i >= 0; i--) {
                                        if (kfs[i].frame <= cur) {
                                            s = kfs[i].frame;
                                            foundStart = true;
                                            if (i + 1 < kfs.length) e = kfs[i + 1].frame;
                                            else e = totalDur;
                                            break;
                                        }
                                    }
                                    if (!foundStart) { e = kfs[0].frame; s = 0; }
                                    return {"start": s, "end": e};
                                }

                                function getInterpAt(f) {
                                    if (!kfs) return "linear";
                                    for (var i = 0; i < kfs.length; i++) {
                                        if (kfs[i].frame === f) return kfs[i].interp || "linear";
                                    }
                                    return "linear";
                                }

                                function updateParam(frame, val) {
                                    if (!hasKeyframes) {
                                        TimelineBridge.updateClipEffectParam(targetClipId, effIdx, key, val);
                                        return;
                                    }
                                    let type = "linear";
                                    if (frame === startFrame) type = getInterpAt(startFrame);
                                    else type = getInterpAt(frame);
                                    if (type === "constant") type = "linear";
                                    effectModel.setKeyframe(key, frame, val, {"interp": type});
                                }

                                Layout.fillWidth: true
                                spacing: 0

                                // 数値パラメータ (ParamControl)
                                Common.ParamControl {
                                    Layout.fillWidth: true
                                    Layout.margins: 4
                                    paramName: {
                                        var interpLabel = {
                                            "linear": " (直線)",
                                            "ease_in": " (加速)",
                                            "ease_out": " (減速)",
                                            "ease_in_out": " (加減速)",
                                            "bezier": " (ベジェ)"
                                        };
                                        return key + (isMoving ? (interpLabel[interpType] || "") : "");
                                    }
                                    startValue: Number(startVal) || 0
                                    endValue: Number(endVal) || 0
                                    minValue: (key === "scale" || key === "opacity") ? 0 : -1000
                                    maxValue: (key === "scale") ? 500 : (key === "opacity" ? 1 : 1000)
                                    decimals: 2
                                    enabled: isNumber
                                    isRangeMode: isMoving
                                    visible: isNumber
                                    
                                    onStartValueModified: function(val) {
                                        root.inputting = true;
                                        updateParam(startFrame, val);
                                        root.inputting = false;
                                    }
                                    onEndValueModified: function(val) {
                                        root.inputting = true;
                                        updateParam(endFrame, val);
                                        root.inputting = false;
                                    }
                                    onParamButtonClicked: {
                                        if (!hasKeyframes) {
                                            effectModel.setKeyframe(key, startFrame, startVal, {"interp": "linear"});
                                            effectModel.setKeyframe(key, endFrame, endVal, {"interp": "linear"});
                                        }
                                        var dialog = easingDialogComponent.createObject(root, {
                                            "effectModel": effectModel,
                                            "paramName": key,
                                            "keyframeFrame": startFrame
                                        });
                                        dialog.open();
                                    }
                                }

                                // 非数値パラメータ (TextField)
                                TextField {
                                    visible: !isNumber
                                    Layout.fillWidth: true
                                    Layout.margins: 4
                                    text: String(effVal)
                                    selectByMouse: true
                                    onEditingFinished: {
                                        root.inputting = true;
                                        TimelineBridge.updateClipEffectParam(targetClipId, effIdx, key, text);
                                        root.inputting = false;
                                    }
                                }

                                // --- Mini Timeline Bar ---
                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 10
                                    Layout.leftMargin: 4
                                    Layout.rightMargin: 4
                                    visible: isNumber

                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: parent.width
                                        height: 2
                                        color: "#555"

                                        // Highlight active interval
                                        Rectangle {
                                            height: 4
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: "#4a9eff"
                                            opacity: 0.7
                                            x: (startFrame / clipDur) * parent.width
                                            width: Math.max(0, ((endFrame - startFrame) / clipDur) * parent.width)
                                            visible: clipDur > 0
                                        }

                                    }

                                    // Keyframes
                                    Repeater {
                                        model: kfs

                                        Rectangle {
                                            width: 4
                                            height: 4
                                            radius: 2
                                            color: "white"
                                            anchors.verticalCenter: parent.verticalCenter
                                            x: (modelData.frame / clipDur) * parent.width - 2

                                            MouseArea {
                                                anchors.fill: parent
                                                acceptedButtons: Qt.RightButton
                                                onClicked: effectModel.removeKeyframe(key, modelData.frame)
                                            }

                                        }

                                    }

                                    // Cursor
                                    Rectangle {
                                        width: 1
                                        height: parent.height
                                        color: "red"
                                        x: (curRelFrame / clipDur) * parent.width
                                        visible: clipDur > 0
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.LeftButton
                                        onDoubleClicked: (mouse) => {
                                            let f = Math.round((mouse.x / width) * clipDur);
                                            let val = effectModel.evaluatedParam(key, f);
                                            effectModel.setKeyframe(key, f, val, {
                                                "interp": "linear"
                                            });
                                        }
                                    }

                                }
                            }
                        }
                    }

                }

            }

        }

    }

    menuBar: MenuBar {
        Menu {
            title: "フィルタ"

            Repeater {
                model: TimelineBridge ? TimelineBridge.getAvailableEffects() : []

                MenuItem {
                    text: modelData.name
                    onTriggered: {
                        if (TimelineBridge) {
                            TimelineBridge.addEffect(targetClipId, modelData.id);
                            reload();
                        }
                    }
                }

            }

        }

    }

}
