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
    property var audioEffectsModel: []
    property bool inputting: false // 入力中フラグ（reloadループ防止用）

    function reload() {
        if (!TimelineBridge || !TimelineBridge.selection)
            return ;

        var id = TimelineBridge.selection.selectedClipId;
        targetClipId = id;
        if (id >= 0) {
            effectsModel = TimelineBridge.getClipEffectsModel(id);
            audioEffectsModel = TimelineBridge.getClipEffectStack(id);
        } else {
            effectsModel = [];
            audioEffectsModel = [];
        }
    }

    // UI定義を正規化してリストとして取得するヘルパー
    function getUiModel(effectModel) {
        var ui = effectModel.uiDefinition;
        if (ui) {
            // 1. 標準形式：{ controls: [...] } または { params: [...] }
            if (ui.controls && typeof ui.controls.length === 'number')
                return ui.controls;

            if (ui.params && typeof ui.params.length === 'number')
                return ui.params;

            // 2. マップ形式 (後方互換): { "param": { ... } }
            var keys = Object.keys(ui);
            if (keys.length > 0 && keys.indexOf("controls") === -1 && keys.indexOf("group") === -1) {
                var list = [];
                for (var i = 0; i < keys.length; i++) {
                    var def = ui[keys[i]];
                    if (typeof def === 'object') {
                        def.param = keys[i];
                        list.push(def);
                    }
                }
                return list;
            }
        }
        // 3. paramsから自動生成
        var params = effectModel.params;
        var pKeys = Object.keys(params).sort();
        var autoList = [];
        for (var j = 0; j < pKeys.length; j++) {
            autoList.push({
                "type": typeof params[pKeys[j]] === 'number' ? 'float' : 'string',
                "param": pKeys[j],
                "label": pKeys[j]
            });
        }
        return autoList;
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

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 1

            Repeater {
                model: effectsModel

                delegate: ColumnLayout {
                    id: effectRoot

                    // 親デリゲートでデータとインデックスを公開
                    property int effectIndex: index
                    property var effectModel: modelData
                    property var currentParams: effectModel ? effectModel.params : ({
                    })

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
                            visible: modelData.category === "filter"
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            flat: true
                            width: 24
                            height: 24
                            onClicked: TimelineBridge.removeEffect(targetClipId, effectRoot.effectIndex)

                            contentItem: Common.RinaIcon {
                                iconName: "close_line"
                                size: 16
                                color: parent.hovered ? "red" : parent.palette.text
                            }

                        }

                    }

                    // 全パラメータ（統一処理）
                    Repeater {
                        model: getUiModel(effectModel)

                        delegate: ColumnLayout {
                            property var def: modelData
                            property string key: def.param || def.name
                            property var effVal: effectRoot.currentParams[key]
                            property bool isNumber: typeof effVal === "number" && (!def.type || ["float", "number", "slider", "spinner"].indexOf(def.type) !== -1)
                            property var effectModel: effectRoot.effectModel
                            property int effIdx: effectRoot.effectIndex
                            // キーフレーム
                            property int curRelFrame: (TimelineBridge && TimelineBridge.transport) ? (TimelineBridge.transport.currentFrame - TimelineBridge.clipStartFrame) : 0
                            property int clipDur: TimelineBridge ? TimelineBridge.clipDurationFrames : 100
                            property var tracks: effectModel ? effectModel.keyframeTracks : null
                            property var track: (tracks && key) ? tracks[key] : null
                            property var kfs: track || []
                            property bool hasKeyframes: kfs.length > 0
                            property var interval: findInterval(kfs, curRelFrame, clipDur)
                            property int startFrame: interval.start
                            property int endFrame: interval.end
                            property var startVal: isNumber ? (effectModel ? effectModel.evaluatedParam(key, startFrame) : effVal) : effVal
                            property var endVal: isNumber ? (effectModel ? effectModel.evaluatedParam(key, endFrame) : effVal) : effVal
                            property string interpType: hasKeyframes ? getInterpAt(startFrame) : "constant"
                            property bool isMoving: isNumber && (hasKeyframes || interpType !== "constant")

                            function hasKeyframeAt(f) {
                                if (!kfs)
                                    return false;

                                for (var i = 0; i < kfs.length; i++) {
                                    if (kfs[i].frame === f)
                                        return true;

                                }
                                return false;
                            }

                            function ensureKeyframeAt(f) {
                                if (!effectModel || !key)
                                    return ;

                                if (hasKeyframeAt(f))
                                    return ;

                                var raw = effectModel.evaluatedParam(key, f);
                                var v = (raw !== undefined && raw !== null) ? raw : effVal;
                                var interp = (def.type === "color") ? "constant" : "linear";
                                effectModel.setKeyframe(key, f, v, {
                                    "interp": interp
                                });
                            }

                            function ensureRangeKeyframes() {
                                ensureKeyframeAt(startFrame);
                                ensureKeyframeAt(endFrame);
                            }

                            function findInterval(kfs, cur, totalDur) {
                                let s = 0, e = totalDur;
                                if (!kfs || kfs.length === 0)
                                    return {
                                    "start": s,
                                    "end": e
                                };

                                let foundStart = false;
                                for (let i = kfs.length - 1; i >= 0; i--) {
                                    if (kfs[i].frame <= cur) {
                                        s = kfs[i].frame;
                                        foundStart = true;
                                        if (i + 1 < kfs.length)
                                            e = kfs[i + 1].frame;
                                        else
                                            e = totalDur;
                                        break;
                                    }
                                }
                                if (!foundStart) {
                                    e = kfs[0].frame;
                                    s = 0;
                                }
                                return {
                                    "start": s,
                                    "end": e
                                };
                            }

                            function getInterpAt(f) {
                                if (!kfs)
                                    return "linear";

                                for (var i = 0; i < kfs.length; i++) {
                                    if (kfs[i].frame === f)
                                        return kfs[i].interp || "linear";

                                }
                                return "linear";
                            }

                            function updateParam(frame, val) {
                                if (!effectModel || !key)
                                    return ;

                                if (!hasKeyframes) {
                                    TimelineBridge.updateClipEffectParam(targetClipId, effIdx, key, val);
                                    return ;
                                }
                                let type = "linear";
                                if (frame === startFrame)
                                    type = getInterpAt(startFrame);
                                else
                                    type = getInterpAt(frame);
                                if (type === "constant")
                                    type = "linear";

                                effectModel.setKeyframe(key, frame, val, {
                                    "interp": type
                                });
                            }

                            Layout.fillWidth: true
                            spacing: 0

                            // 数値
                            Common.ParamControl {
                                Layout.fillWidth: true
                                Layout.margins: 4
                                visible: isNumber
                                enabled: isNumber
                                isRangeMode: isMoving
                                paramName: {
                                    var interpLabel = {
                                        "linear": " (直線)",
                                        "ease_in": " (加速)",
                                        "ease_out": " (減速)",
                                        "ease_in_out": " (加減速)",
                                        "bezier": " (ベジェ)"
                                    };
                                    var name = (def.label && def.label !== "") ? def.label : key;
                                    return name + (isMoving ? (interpLabel[interpType] || "") : "");
                                }
                                startValue: Number(startVal) || 0
                                endValue: Number(endVal) || 0
                                minValue: (def.min !== undefined) ? def.min : ((key === "scale" || key === "opacity") ? 0 : -1000)
                                maxValue: (def.max !== undefined) ? def.max : ((key === "scale") ? 500 : (key === "opacity" ? 1 : 1000))
                                decimals: (def.decimals !== undefined) ? def.decimals : 2
                                onStartValueModified: function(val) {
                                    root.inputting = true;
                                    ensureRangeKeyframes();
                                    updateParam(startFrame, val);
                                    root.inputting = false;
                                }
                                onEndValueModified: function(val) {
                                    root.inputting = true;
                                    ensureRangeKeyframes();
                                    updateParam(endFrame, val);
                                    root.inputting = false;
                                }
                                onParamButtonClicked: {
                                    if (!effectModel || !key)
                                        return ;

                                    // 区間キーフレームがない場合は生成
                                    ensureRangeKeyframes();
                                    var dialog = easingDialogComponent.createObject(root, {
                                        "clipId": targetClipId,
                                        "effectIndex": effIdx,
                                        "effectModel": effectModel,
                                        "paramName": key,
                                        "keyframeFrame": startFrame
                                    });
                                    if (dialog)
                                        dialog.open();

                                }
                            }

                            // 非数値 (ControlLoader で型別UI)
                            Common.ControlLoader {
                                Layout.fillWidth: true
                                Layout.margins: 4
                                visible: !isNumber
                                definition: def
                                value: effVal
                                effectRootRef: effectRoot
                                onValueModified: function(val) {
                                    root.inputting = true;
                                    TimelineBridge.updateClipEffectParam(targetClipId, effIdx, key, val);
                                    root.inputting = false;
                                }
                                onParamButtonClicked: {
                                    if (!effectModel || !key)
                                        return ;

                                    ensureRangeKeyframes();
                                    var dialog = easingDialogComponent.createObject(root, {
                                        "clipId": targetClipId,
                                        "effectIndex": effIdx,
                                        "effectModel": effectModel,
                                        "paramName": key,
                                        "keyframeFrame": startFrame
                                    });
                                    if (dialog)
                                        dialog.open();

                                }
                            }

                            // ミニタイムラインバー
                            Item {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 12
                                Layout.leftMargin: 4
                                Layout.rightMargin: 4
                                visible: isNumber

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: parent.width
                                    height: 2
                                    color: "#555"

                                    Rectangle {
                                        height: 4
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: "#48f"
                                        opacity: 0.7
                                        x: (startFrame / clipDur) * parent.width
                                        width: Math.max(0, ((endFrame - startFrame) / clipDur) * parent.width)
                                        visible: clipDur > 0
                                    }

                                }

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

            // オーディオプラグインのパラメータ表示
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                visible: TimelineBridge && TimelineBridge.isAudioClip(targetClipId)

                Repeater {
                    model: audioEffectsModel

                    delegate: ColumnLayout {
                        id: audioEffectRoot

                        property int effectIndex: index

                        Layout.fillWidth: true
                        spacing: 0

                        Rectangle {
                            Layout.fillWidth: true
                            height: 24
                            color: palette.midlight

                            Label {
                                text: modelData.name + " (" + modelData.format + ")"
                                color: palette.text
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                            }

                            // 削除ボタン
                            Button {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                flat: true
                                width: 24
                                height: 24
                                onClicked: TimelineBridge.removeAudioPlugin(targetClipId, audioEffectRoot.effectIndex)

                                contentItem: Common.RinaIcon {
                                    iconName: "close_line"
                                    size: 16
                                    color: parent.hovered ? "red" : parent.palette.text
                                }

                            }

                        }

                        Repeater {
                            model: TimelineBridge.getEffectParameters(targetClipId, index)

                            delegate: Common.ControlLoader {
                                Layout.fillWidth: true
                                Layout.margins: 4
                                definition: ({
                                    "type": modelData.type || "slider",
                                    "label": modelData.name,
                                    "min": modelData.min,
                                    "max": modelData.max
                                })
                                value: modelData.current
                                onValueModified: (newValue) => {
                                    TimelineBridge.setEffectParameter(targetClipId, audioEffectRoot.effectIndex, modelData.pIdx, newValue);
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
            id: filterMenu

            title: "フィルタ"

            Repeater {
                model: (TimelineBridge && !TimelineBridge.isAudioClip(targetClipId)) ? TimelineBridge.getAvailableEffects() : []

                Common.IconMenuItem {
                    text: modelData.name
                    iconName: "magic_line"
                    onTriggered: {
                        if (TimelineBridge)
                            TimelineBridge.addEffect(targetClipId, modelData.id);

                    }
                }

            }

            // カテゴリごとのサブメニューを生成
            Instantiator {
                model: (targetClipId !== -1 && TimelineBridge && TimelineBridge.isAudioClip(targetClipId)) ? TimelineBridge.getPluginCategories() : []
                onObjectAdded: (index, object) => {
                    return filterMenu.insertMenu(index, object);
                }
                onObjectRemoved: (index, object) => {
                    return filterMenu.removeMenu(object);
                }

                delegate: Menu {
                    id: categoryMenu

                    // プラグイン一覧をキャッシュするためのプロパティ
                    property var cachedPlugins: []

                    title: modelData // カテゴリ名
                    // サブメニューが開かれる直前に一度だけモデルをロードする（遅延評価）
                    onAboutToShow: {
                        if (cachedPlugins.length === 0 && TimelineBridge)
                            cachedPlugins = TimelineBridge.getPluginsByCategory(categoryMenu.title);

                    }

                    Instantiator {
                        model: categoryMenu.cachedPlugins
                        onObjectAdded: (index, subObj) => {
                            // eslint-disable-line
                            return categoryMenu.insertItem(index, subObj);
                        }
                        onObjectRemoved: (index, subObj) => {
                            return categoryMenu.removeItem(subObj);
                        }

                        delegate: Common.IconMenuItem {
                            text: modelData.name
                            iconName: "music_line"
                            onTriggered: {
                                if (TimelineBridge)
                                    TimelineBridge.addAudioPlugin(targetClipId, modelData.id);

                            }
                        }

                    }

                }

            }

        }

    }

}
