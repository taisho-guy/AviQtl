import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    // keyframeFrame は変更しない（元フレームの interp をUIに正しく表示する）

    id: root

    property int clipId: -1
    property int effectIndex: -1
    property var effectModel: null
    property string paramName: ""
    property int keyframeFrame: 0
    property string selectedType: "none"
    property int stepFrames: 1
    property var bezierParams: [0.33, 0, 0.66, 1, 1, 1]
    property real previewScale: 1 // 0.25 ～ 4.0
    property real previewOffsetX: 0 // 論理座標 (0-1空間) での平行移動
    property real previewOffsetY: 0
    property bool isInitializing: false

    function requestPreview() {
        if (previewCanvas)
            previewCanvas.requestPaint();

    }

    function evalEasing(t) {
        return evalEasingByType(t, root.selectedType);
    }

    function evalEasingByType(t, type) {
        function _bounceOut(x) {
            var n1 = 7.5625, d1 = 2.75;
            if (x < 1 / d1)
                return n1 * x * x;

            if (x < 2 / d1) {
                x -= 1.5 / d1;
                return n1 * x * x + 0.75;
            }
            if (x < 2.5 / d1) {
                x -= 2.25 / d1;
                return n1 * x * x + 0.9375;
            }
            x -= 2.625 / d1;
            return n1 * x * x + 0.984375;
        }

        var bz = root.bezierParams;
        if (type === "linear")
            return t;

        if (type === "ease_in_sine")
            return 1 - Math.cos(t * Math.PI / 2);

        if (type === "ease_out_sine")
            return Math.sin(t * Math.PI / 2);

        if (type === "ease_in_out_sine")
            return -(Math.cos(Math.PI * t) - 1) / 2;

        if (type === "ease_out_in_sine")
            return t < 0.5 ? Math.sin(t * Math.PI) / 2 : (1 - Math.cos((t * 2 - 1) * Math.PI / 2)) / 2 + 0.5;

        if (type === "ease_in_quad")
            return t * t;

        if (type === "ease_out_quad")
            return 1 - (1 - t) * (1 - t);

        if (type === "ease_in_out_quad")
            return t < 0.5 ? 2 * t * t : 1 - ((-2 * t + 2) * (-2 * t + 2)) / 2;

        if (type === "ease_out_in_quad")
            return t < 0.5 ? (1 - (1 - 2 * t) ** 2) / 2 : (2 * t - 1) ** 2 / 2 + 0.5;

        if (type === "ease_in_cubic")
            return t * t * t;

        if (type === "ease_out_cubic")
            return 1 - (1 - t) ** 3;

        if (type === "ease_in_out_cubic")
            return t < 0.5 ? 4 * t ** 3 : 1 - ((-2 * t + 2) ** 3) / 2;

        if (type === "ease_out_in_cubic")
            return t < 0.5 ? (1 - (1 - 2 * t) ** 3) / 2 : (2 * t - 1) ** 3 / 2 + 0.5;

        if (type === "ease_in_quart")
            return t ** 4;

        if (type === "ease_out_quart")
            return 1 - (1 - t) ** 4;

        if (type === "ease_in_out_quart")
            return t < 0.5 ? 8 * t ** 4 : 1 - ((-2 * t + 2) ** 4) / 2;

        if (type === "ease_out_in_quart")
            return t < 0.5 ? (1 - (1 - 2 * t) ** 4) / 2 : (2 * t - 1) ** 4 / 2 + 0.5;

        if (type === "ease_out_in_quart")
            return t < 0.5 ? (1 - (1 - 2 * t) ** 4) / 2 : (2 * t - 1) ** 4 / 2 + 0.5;

        if (type === "ease_in_quint")
            return t ** 5;

        if (type === "ease_out_quint")
            return 1 - (1 - t) ** 5;

        if (type === "ease_in_out_quint")
            return t < 0.5 ? 16 * t ** 5 : 1 - ((-2 * t + 2) ** 5) / 2;

        if (type === "ease_out_in_quint")
            return t < 0.5 ? (1 - (1 - 2 * t) ** 5) / 2 : (2 * t - 1) ** 5 / 2 + 0.5;

        if (type === "ease_out_in_quint")
            return t < 0.5 ? (1 - (1 - 2 * t) ** 5) / 2 : (2 * t - 1) ** 5 / 2 + 0.5;

        if (type === "ease_in_expo")
            return t === 0 ? 0 : Math.pow(2, 10 * t - 10);

        if (type === "ease_out_expo")
            return t === 1 ? 1 : 1 - Math.pow(2, -10 * t);

        if (type === "ease_in_out_expo") {
            if (t === 0)
                return 0;

            if (t === 1)
                return 1;

            return t < 0.5 ? Math.pow(2, 20 * t - 10) / 2 : (2 - Math.pow(2, -20 * t + 10)) / 2;
        }
        if (type === "ease_out_in_expo") {
            if (t === 0)
                return 0;

            if (t === 1)
                return 1;

            return t < 0.5 ? (1 - Math.pow(2, -20 * t)) / 2 : Math.pow(2, 20 * t - 20) / 2 + 0.5;
        }
        if (type === "ease_in_circ")
            return 1 - Math.sqrt(1 - t * t);

        if (type === "ease_out_circ")
            return Math.sqrt(1 - (t - 1) ** 2);

        if (type === "ease_in_out_circ")
            return t < 0.5 ? (1 - Math.sqrt(1 - 4 * t * t)) / 2 : (Math.sqrt(1 - (-2 * t + 2) ** 2) + 1) / 2;

        if (type === "ease_out_in_circ")
            return t < 0.5 ? Math.sqrt(1 - (2 * t - 1) ** 2) / 2 : (1 - Math.sqrt(1 - (2 * t - 1) ** 2)) / 2 + 0.5;

        if (type === "ease_in_back") {
            var c1 = 1.70158, c3 = c1 + 1;
            return c3 * t ** 3 - c1 * t ** 2;
        }
        if (type === "ease_out_back") {
            var c1b = 1.70158, c3b = c1b + 1;
            return 1 + c3b * (t - 1) ** 3 + c1b * (t - 1) ** 2;
        }
        if (type === "ease_in_out_back") {
            var c2 = 1.70158 * 1.525;
            return t < 0.5 ? ((2 * t) ** 2 * ((c2 + 1) * 2 * t - c2)) / 2 : ((2 * t - 2) ** 2 * ((c2 + 1) * (2 * t - 2) + c2) + 2) / 2;
        }
        if (type === "ease_out_in_back") {
            var c1 = 1.70158, c3 = c1 + 1;
            var eout = (u) => {
                return 1 + c3 * (u - 1) ** 3 + c1 * (u - 1) ** 2;
            };
            var ein = (u) => {
                return c3 * u ** 3 - c1 * u ** 2;
            };
            return t < 0.5 ? eout(2 * t) / 2 : ein(2 * t - 1) / 2 + 0.5;
        }
        if (type === "ease_in_elastic") {
            var c4 = 2 * Math.PI / 3;
            if (t === 0)
                return 0;

            if (t === 1)
                return 1;

            return -Math.pow(2, 10 * t - 10) * Math.sin((10 * t - 10.75) * c4);
        }
        if (type === "ease_out_elastic") {
            var c4e = 2 * Math.PI / 3;
            if (t === 0)
                return 0;

            if (t === 1)
                return 1;

            return Math.pow(2, -10 * t) * Math.sin((10 * t - 0.75) * c4e) + 1;
        }
        if (type === "ease_in_out_elastic") {
            var c5 = 2 * Math.PI / 4.5;
            if (t === 0)
                return 0;

            if (t === 1)
                return 1;

            return t < 0.5 ? -(Math.pow(2, 20 * t - 10) * Math.sin((20 * t - 11.125) * c5)) / 2 : (Math.pow(2, -20 * t + 10) * Math.sin((20 * t - 11.125) * c5)) / 2 + 1;
        }
        if (type === "ease_out_in_elastic") {
            var c4 = 2 * Math.PI / 3;
            if (t === 0)
                return 0;

            if (t === 1)
                return 1;

            var eout = (u) => {
                return Math.pow(2, -10 * u) * Math.sin((u * 10 - 0.75) * c4) + 1;
            };
            var ein = (u) => {
                return -Math.pow(2, 10 * u - 10) * Math.sin((u * 10 - 10.75) * c4);
            };
            return t < 0.5 ? eout(2 * t) / 2 : ein(2 * t - 1) / 2 + 0.5;
        }
        if (type === "ease_out_bounce")
            return _bounceOut(t);

        if (type === "ease_in_bounce")
            return 1 - _bounceOut(1 - t);

        if (type === "ease_in_out_bounce")
            return t < 0.5 ? (1 - _bounceOut(1 - 2 * t)) / 2 : (1 + _bounceOut(2 * t - 1)) / 2;

        if (type === "ease_out_in_bounce")
            return t < 0.5 ? _bounceOut(2 * t) / 2 : (1 - _bounceOut(1 - 2 * (t - 0.5))) / 2 + 0.5;

        if (type === "custom") {
            var prevX = 0, prevY = 0;
            for (var i = 0; i < bz.length; i += 6) {
                var cp1x = bz[i], cp1y = bz[i + 1], cp2x = bz[i + 2], cp2y = bz[i + 3], endX = bz[i + 4], endY = bz[i + 5];
                if (t <= endX || i + 6 >= bz.length) {
                    var range = endX - prevX;
                    if (range < 0.0001)
                        return endY;

                    var nx = (t - prevX) / range, nc1x = (cp1x - prevX) / range, nc2x = (cp2x - prevX) / range, T = nx;
                    for (var k = 0; k < 8; k++) {
                        var u = 1 - T, cx = 3 * u * u * T * nc1x + 3 * u * T * T * nc2x + T * T * T, err = cx - nx;
                        if (Math.abs(err) < 0.0001)
                            break;

                        var dcx = 3 * u * u * nc1x + 6 * u * T * (nc2x - nc1x) + 3 * T * T * (1 - nc2x);
                        if (Math.abs(dcx) < 1e-06)
                            break;

                        T -= err / dcx;
                    }
                    T = Math.max(0, Math.min(1, T));
                    var u2 = 1 - T;
                    return u2 ** 3 * prevY + 3 * u2 ** 2 * T * cp1y + 3 * u2 * T ** 2 * cp2y + T ** 3 * endY;
                }
                prevX = endX;
                prevY = endY;
            }
        }
        return t;
    }

    function openConfig(args) {
        isInitializing = true;
        clipId = args.clipId;
        effectIndex = args.effectIndex;
        effectModel = args.effectModel;
        paramName = args.paramName;
        keyframeFrame = args.keyframeFrame;
        show();
        raise();
        requestActivate();
        previewScale = 1;
        previewOffsetX = 0;
        previewOffsetY = 0;
        // 初回オープン時: 最終フレームにキーフレームがなければ通常の中間点として自動追加
        {
            const _clipDur = Workspace.currentTimeline ? Workspace.currentTimeline.clipDurationFrames : 100;
            const _fps = (Workspace.currentTimeline && Workspace.currentTimeline.project) ? Workspace.currentTimeline.project.fps : 60;
            const _endFrame = _clipDur;
            const _track = effectModel.keyframeListForUi(paramName) || [];
            const _hasKfAtEnd = _track.some(function(kf) {
                return kf.frame === _endFrame;
            });
            if (!_hasKfAtEnd) {
                const _endVal = effectModel.evaluatedParam(paramName, _endFrame, _fps);
                // 末尾の既存キーフレーム（= 末尾セグメントの左端）の interp を linear に更新
                // これにより 左端フレーム→endFrame が直線補間になる
                if (_track.length > 0) {
                    const _prevKf = _track[_track.length - 1];
                    Workspace.currentTimeline.setKeyframe(clipId, effectIndex, paramName, _prevKf.frame, _prevKf.value, {
                        "interp": "linear"
                    });
                }
                // endFrame 自体は末尾なので interp は使われない（none のまま）
                Workspace.currentTimeline.setKeyframe(clipId, effectIndex, paramName, _endFrame, _endVal, {
                    "interp": "none"
                });
                // 初回設定直後に UI の補間状態を確定させる（右値変更なしでも補間が反映されるよう）
                Qt.callLater(function() {
                    if (!isInitializing)
                        updateKeyframe();

                });
            }
        };
        typeCombo.model = effectModel.availableEasings();
        const tracks = effectModel.keyframeTracks;
        const track = effectModel ? effectModel.keyframeListForUi(paramName) : undefined;
        if (!track)
            return ;

        for (let i = 0; i < track.length; i++) {
            if (track[i].frame !== keyframeFrame)
                continue;

            const kf = track[i];
            selectedType = kf.interp || "none";
            stepFrames = (kf.modeParams && kf.modeParams.stepFrames) ? kf.modeParams.stepFrames : 1;
            if (selectedType === "bezier")
                selectedType = "custom";

            var idx = typeCombo.model.indexOf(selectedType);
            typeCombo.currentIndex = idx >= 0 ? idx : 0;
            if (selectedType === "custom") {
                if (kf.points && kf.points.length >= 6) {
                    var pts = [];
                    for (let j = 0; j < kf.points.length; j++) pts.push(kf.points[j])
                    bezierParams = pts;
                } else {
                    bezierParams = [kf.bzx1 !== undefined ? kf.bzx1 : 0.33, kf.bzy1 !== undefined ? kf.bzy1 : 0, kf.bzx2 !== undefined ? kf.bzx2 : 0.66, kf.bzy2 !== undefined ? kf.bzy2 : 1, 1, 1];
                }
            }
            break;
        }
        requestPreview();
        isInitializing = false;
    }

    function updateKeyframe() {
        if (!effectModel)
            return ;

        const kf = effectModel.keyframeListForUi(paramName).find((k) => {
            return k.frame === keyframeFrame;
        });
        if (!kf)
            return ;

        let options = {
            "interp": selectedType
        };
        if (selectedType === "custom") {
            var pts = [];
            for (let j = 0; j < bezierParams.length; j++) pts.push(bezierParams[j])
            options.points = pts;
        }
        if (selectedType === "random" || selectedType === "alternate")
            options.modeParams = {
            "stepFrames": Math.max(1, stepFrames)
        };

        Workspace.currentTimeline.setKeyframe(clipId, effectIndex, paramName, keyframeFrame, kf.value, options);
    }

    title: qsTr("補間設定: %1").arg(paramName)
    width: 820
    height: 540
    onSelectedTypeChanged: {
        requestPreview();
        if (!isInitializing)
            updateKeyframe();

    }
    onBezierParamsChanged: {
        requestPreview();
        if (!isInitializing)
            updateKeyframe();

    }
    onPreviewScaleChanged: requestPreview()
    onPreviewOffsetXChanged: requestPreview()
    onPreviewOffsetYChanged: requestPreview()

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        Item {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 360
            SplitView.preferredWidth: 520

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                // ズームコントロール
                RowLayout {
                    spacing: 6

                    Label {
                        text: qsTr("プレビュー")
                        font.bold: true
                        color: palette.text
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("ズーム:")
                        font.pixelSize: 11
                        color: palette.mid
                    }

                    Button {
                        text: "−"
                        flat: true
                        implicitWidth: 24
                        implicitHeight: 24
                        onClicked: root.previewScale = Math.max(0.25, root.previewScale / 1.4)
                    }

                    Label {
                        text: Math.round(root.previewScale * 100) + "%"
                        font.pixelSize: 11
                        color: palette.text
                        Layout.preferredWidth: 36
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Button {
                        text: "+"
                        flat: true
                        implicitWidth: 24
                        implicitHeight: 24
                        onClicked: root.previewScale = Math.min(4, root.previewScale * 1.4)
                    }

                    Button {
                        text: "1:1"
                        flat: true
                        font.pixelSize: 10
                        implicitWidth: 30
                        implicitHeight: 24
                        onClicked: {
                            root.previewScale = 1;
                            root.previewOffsetX = 0;
                            root.previewOffsetY = 0;
                        }
                    }

                }

                // プレビューキャンバス本体
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: palette.base
                    border.color: palette.mid
                    border.width: 1
                    radius: 4
                    clip: true

                    Canvas {
                        id: previewCanvas

                        // 論理座標 → ピクセル変換（ズーム・パン考慮）
                        function lxToPx(lx) {
                            var scale = root.previewScale;
                            var offX = root.previewOffsetX;
                            return (lx - offX) * scale * width + width * (1 - scale) / 2 + width * offX * scale;
                        }

                        function lyToPy(ly) {
                            var scale = root.previewScale;
                            var offY = root.previewOffsetY;
                            return height - ((ly - offY) * scale * height + height * (1 - scale) / 2 + height * offY * scale);
                        }

                        // ピクセル → 論理座標（ドラッグ用）
                        function pxToLx(px) {
                            var scale = root.previewScale;
                            return (px - width * (1 - scale) / 2) / (scale * width) + root.previewOffsetX * (1 - 1 / scale);
                        }

                        function pyToLy(py) {
                            var scale = root.previewScale;
                            return 1 - ((py - height * (1 - scale) / 2) / (scale * height) + root.previewOffsetY * (1 - 1 / scale));
                        }

                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d");
                            var w = width, h = height;
                            ctx.clearRect(0, 0, w, h);
                            ctx.fillStyle = "#1e1e1e";
                            ctx.fillRect(0, 0, w, h);
                            // グリッド (論理0-1の格子)
                            ctx.strokeStyle = "#333";
                            ctx.lineWidth = 0.5;
                            for (var gi = 0; gi <= 4; gi++) {
                                var gx = lxToPx(gi / 4), gy = lyToPy(gi / 4);
                                ctx.beginPath();
                                ctx.moveTo(gx, 0);
                                ctx.lineTo(gx, h);
                                ctx.stroke();
                                ctx.beginPath();
                                ctx.moveTo(0, gy);
                                ctx.lineTo(w, gy);
                                ctx.stroke();
                            }
                            // 有効領域枠 (0-1 矩形)
                            ctx.strokeStyle = "#555";
                            ctx.lineWidth = 1;
                            var x0 = lxToPx(0), x1 = lxToPx(1), y0 = lyToPy(0), y1 = lyToPy(1);
                            ctx.strokeRect(x0, y1, x1 - x0, y0 - y1);
                            // 対角線 (linear 参照)
                            ctx.strokeStyle = "#444";
                            ctx.lineWidth = 1;
                            ctx.beginPath();
                            ctx.moveTo(x0, y0);
                            ctx.lineTo(x1, y1);
                            ctx.stroke();
                            // カーブ本体
                            ctx.strokeStyle = "#4488ff";
                            ctx.lineWidth = 2;
                            ctx.beginPath();
                            var steps = 128;
                            for (var s = 0; s <= steps; s++) {
                                var t = s / steps;
                                var y = root.evalEasing(t);
                                var px = lxToPx(t);
                                var py = lyToPy(y);
                                s === 0 ? ctx.moveTo(px, py) : ctx.lineTo(px, py);
                            }
                            ctx.stroke();
                            // custom モード: タンジェントラインとハンドル描画
                            if (root.selectedType === "custom") {
                                var bz = root.bezierParams;
                                var prevAX = lxToPx(0), prevAY = lyToPy(0);
                                for (var seg = 0; seg < bz.length; seg += 6) {
                                    var c1px = lxToPx(bz[seg]), c1py = lyToPy(bz[seg + 1]);
                                    var c2px = lxToPx(bz[seg + 2]), c2py = lyToPy(bz[seg + 3]);
                                    var enpx = lxToPx(bz[seg + 4]), enpy = lyToPy(bz[seg + 5]);
                                    // タンジェントライン
                                    ctx.strokeStyle = "#888";
                                    ctx.lineWidth = 0.8;
                                    ctx.setLineDash([3, 3]);
                                    ctx.beginPath();
                                    ctx.moveTo(prevAX, prevAY);
                                    ctx.lineTo(c1px, c1py);
                                    ctx.stroke();
                                    ctx.beginPath();
                                    ctx.moveTo(enpx, enpy);
                                    ctx.lineTo(c2px, c2py);
                                    ctx.stroke();
                                    ctx.setLineDash([]);
                                    // cp1
                                    ctx.fillStyle = "#fff";
                                    ctx.beginPath();
                                    ctx.arc(c1px, c1py, 5, 0, 2 * Math.PI);
                                    ctx.fill();
                                    // cp2
                                    ctx.beginPath();
                                    ctx.arc(c2px, c2py, 5, 0, 2 * Math.PI);
                                    ctx.fill();
                                    // anchor (中間のみ)
                                    if (seg + 6 < bz.length) {
                                        ctx.fillStyle = "#ffdd00";
                                        ctx.beginPath();
                                        ctx.arc(enpx, enpy, 6, 0, 2 * Math.PI);
                                        ctx.fill();
                                    }
                                    prevAX = enpx;
                                    prevAY = enpy;
                                }
                            }
                            // 座標軸ラベル
                            ctx.fillStyle = "#666";
                            ctx.font = "10px sans-serif";
                            ctx.fillText("0", lxToPx(-0.03), lyToPy(-0.04));
                            ctx.fillText("1", lxToPx(0.97), lyToPy(-0.04));
                            ctx.fillText("1", lxToPx(-0.06), lyToPy(1.02));
                        }
                    }

                    MouseArea {
                        id: panArea

                        property real lastX: 0
                        property real lastY: 0

                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        cursorShape: Qt.ClosedHandCursor
                        onPressed: (mouse) => {
                            lastX = mouse.x;
                            lastY = mouse.y;
                        }
                        onPositionChanged: (mouse) => {
                            if (!(mouse.buttons & Qt.RightButton))
                                return ;

                            var scale = root.previewScale;
                            root.previewOffsetX -= (mouse.x - lastX) / (scale * previewCanvas.width);
                            root.previewOffsetY += (mouse.y - lastY) / (scale * previewCanvas.height);
                            lastX = mouse.x;
                            lastY = mouse.y;
                        }
                        onWheel: (wheel) => {
                            var factor = wheel.angleDelta.y > 0 ? 1.2 : 1 / 1.2;
                            root.previewScale = Math.max(0.25, Math.min(4, root.previewScale * factor));
                            wheel.accepted = true;
                        }
                    }

                    MouseArea {
                        id: dragArea

                        property int dragIdx: -1

                        function findNearest(mx, my) {
                            var bz = root.bezierParams;
                            var best = -1, bestD = 14 * 14;
                            for (var seg = 0; seg < bz.length; seg += 6) {
                                var pts = [[bz[seg], bz[seg + 1]], [bz[seg + 2], bz[seg + 3]], (seg + 6 < bz.length) ? [bz[seg + 4], bz[seg + 5]] : null];
                                var indices = [seg, seg + 2, seg + 4];
                                for (var pi = 0; pi < 3; pi++) {
                                    if (!pts[pi])
                                        continue;

                                    var px = previewCanvas.lxToPx(pts[pi][0]);
                                    var py = previewCanvas.lyToPy(pts[pi][1]);
                                    var dx = mx - px, dy = my - py;
                                    if (dx * dx + dy * dy < bestD) {
                                        bestD = dx * dx + dy * dy;
                                        best = indices[pi];
                                    }
                                }
                            }
                            return best;
                        }

                        anchors.fill: parent
                        enabled: root.selectedType === "custom"
                        acceptedButtons: Qt.LeftButton
                        cursorShape: dragIdx >= 0 ? Qt.CrossCursor : Qt.ArrowCursor
                        onPressed: (mouse) => {
                            dragIdx = findNearest(mouse.x, mouse.y);
                        }
                        onPositionChanged: (mouse) => {
                            if (dragIdx < 0)
                                return ;

                            var lx = previewCanvas.pxToLx(mouse.x);
                            var ly = previewCanvas.pyToLy(mouse.y);
                            // X軸クランプ（X は 0-1 必須、Y は範囲外 OK）
                            var p = root.bezierParams.slice();
                            p[dragIdx] = Math.max(0, Math.min(1, lx));
                            p[dragIdx + 1] = ly;
                            root.bezierParams = p;
                        }
                        onReleased: {
                            dragIdx = -1;
                        }
                    }

                    // ズームヒント
                    Label {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 4
                        text: qsTr("右ドラッグ:パン  ホイール:ズーム") + (root.selectedType === "custom" ? qsTr("  左ドラッグ:ハンドル") : "")
                        font.pixelSize: 9
                        color: palette.mid
                    }

                }

                // custom モード専用: 制御点テキスト入力
                GroupBox {
                    title: qsTr("制御点")
                    enabled: root.selectedType === "custom"
                    opacity: root.selectedType === "custom" ? 1 : 0.35
                    Layout.fillWidth: true

                    GridLayout {
                        columns: 8
                        columnSpacing: 6
                        rowSpacing: 4

                        Repeater {
                            model: [{
                                "label": "CP1 X",
                                "idx": 0,
                                "clamp": true
                            }, {
                                "label": "CP1 Y",
                                "idx": 1,
                                "clamp": false
                            }, {
                                "label": "CP2 X",
                                "idx": 2,
                                "clamp": true
                            }, {
                                "label": "CP2 Y",
                                "idx": 3,
                                "clamp": false
                            }]

                            delegate: RowLayout {
                                spacing: 3

                                Label {
                                    text: modelData.label
                                    font.pixelSize: 11
                                    color: palette.mid
                                }

                                TextField {
                                    id: cpField

                                    property int pIdx: modelData.idx
                                    property bool doClamp: modelData.clamp

                                    implicitWidth: 60
                                    font.pixelSize: 11
                                    text: root.bezierParams[pIdx].toFixed(3)
                                    selectByMouse: true
                                    onEditingFinished: {
                                        var p = root.bezierParams.slice();
                                        var v = parseFloat(text) || 0;
                                        p[pIdx] = doClamp ? Math.max(0, Math.min(1, v)) : v;
                                        root.bezierParams = p;
                                    }

                                    Binding on text {
                                        when: !cpField.activeFocus
                                        value: root.bezierParams[cpField.pIdx].toFixed(3)
                                    }

                                }

                            }

                        }

                    }

                }

            }
            // End of Item 1

        }

        Item {
            SplitView.minimumWidth: 240
            SplitView.preferredWidth: 280

            ColumnLayout {
                // カテゴリ別グリッド
                property string filterText: ""
                // イージングカテゴリ定義
                property var categories: [{
                    "name": qsTr("直線"),
                    "items": ["linear"]
                }, {
                    "name": qsTr("サイン"),
                    "items": ["ease_in_sine", "ease_out_sine", "ease_in_out_sine", "ease_out_in_sine"]
                }, {
                    "name": qsTr("2次"),
                    "items": ["ease_in_quad", "ease_out_quad", "ease_in_out_quad", "ease_out_in_quad"]
                }, {
                    "name": qsTr("3次"),
                    "items": ["ease_in_cubic", "ease_out_cubic", "ease_in_out_cubic", "ease_out_in_cubic"]
                }, {
                    "name": qsTr("4次"),
                    "items": ["ease_in_quart", "ease_out_quart", "ease_in_out_quart", "ease_out_in_quart"]
                }, {
                    "name": qsTr("5次"),
                    "items": ["ease_in_quint", "ease_out_quint", "ease_in_out_quint", "ease_out_in_quint"]
                }, {
                    "name": qsTr("指数"),
                    "items": ["ease_in_expo", "ease_out_expo", "ease_in_out_expo", "ease_out_in_expo"]
                }, {
                    "name": qsTr("円"),
                    "items": ["ease_in_circ", "ease_out_circ", "ease_in_out_circ", "ease_out_in_circ"]
                }, {
                    "name": qsTr("戻る"),
                    "items": ["ease_in_back", "ease_out_back", "ease_in_out_back", "ease_out_in_back"]
                }, {
                    "name": qsTr("弾性"),
                    "items": ["ease_in_elastic", "ease_out_elastic", "ease_in_out_elastic", "ease_out_in_elastic"]
                }, {
                    "name": qsTr("跳ね返り"),
                    "items": ["ease_in_bounce", "ease_out_bounce", "ease_in_out_bounce", "ease_out_in_bounce"]
                }, {
                    "name": qsTr("カスタム"),
                    "items": ["custom"]
                }]

                anchors.fill: parent
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 6

                RowLayout {
                    Label {
                        text: qsTr("種類")
                        font.bold: true
                        color: palette.text
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                    // テキスト検索フィルター

                    TextField {
                        id: searchField

                        placeholderText: qsTr("検索...")
                        implicitWidth: 110
                        font.pixelSize: 11
                        selectByMouse: true
                        // ComboBox は参照用として隠しで保持
                        onTextChanged: easingGrid.filterText = text.toLowerCase().replace(/_/g, "")
                    }
                    // 隠し ComboBox（既存 API 互換用）

                    ComboBox {
                        id: typeCombo

                        visible: false
                        model: ["linear"]
                        onActivated: (idx) => {
                            return root.selectedType = currentText;
                        }
                    }

                }

                ScrollView {
                    id: easingGrid

                    property string filterText: ""

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: easingGrid.availableWidth
                        spacing: 2

                        Repeater {
                            model: easingGrid.parent.categories

                            delegate: ColumnLayout {
                                id: catCol

                                property var catData: modelData
                                // フィルター適用後のアイテム
                                property var visibleItems: {
                                    var ft = easingGrid.filterText;
                                    if (ft === "")
                                        return catData.items;

                                    return catData.items.filter((e) => {
                                        return e.toLowerCase().replace(/_/g, "").includes(ft);
                                    });
                                }

                                visible: visibleItems.length > 0
                                width: parent.width
                                spacing: 1

                                Label {
                                    text: catCol.catData.name
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: palette.mid
                                    leftPadding: 2
                                    topPadding: 4
                                }

                                GridLayout {
                                    columns: 4
                                    columnSpacing: 4
                                    rowSpacing: 4
                                    width: parent.width

                                    Repeater {
                                        model: catCol.visibleItems

                                        delegate: Button {
                                            property string easingName: modelData
                                            property bool isCurrent: root.selectedType === easingName

                                            // ミニプレビューキャンバス付きボタン
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 64
                                            flat: true
                                            padding: 0
                                            onClicked: {
                                                root.selectedType = easingName;
                                                var idx = typeCombo.model.indexOf(easingName);
                                                if (idx >= 0)
                                                    typeCombo.currentIndex = idx;

                                            }

                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.margins: 4
                                                spacing: 1

                                                // ミニプレビュー
                                                Canvas {
                                                    id: miniCanvas

                                                    property string etype: easingName

                                                    // ミニプレビュー専用軽量評価
                                                    function miniEval(t, type) {
                                                        return root.evalEasingByType(t, type);
                                                    }

                                                    Layout.fillWidth: true
                                                    Layout.fillHeight: true
                                                    Component.onCompleted: requestPaint()
                                                    onPaint: {
                                                        var ctx = getContext("2d");
                                                        var w = width, h = height;
                                                        ctx.clearRect(0, 0, w, h);
                                                        ctx.fillStyle = isCurrent ? "#1a2a3a" : "#1a1a1a";
                                                        ctx.fillRect(0, 0, w, h);
                                                        // カーブ
                                                        ctx.strokeStyle = isCurrent ? "#88ccff" : "#4488ff";
                                                        ctx.lineWidth = 1.5;
                                                        ctx.beginPath();
                                                        var steps = 48;
                                                        for (var s = 0; s <= steps; s++) {
                                                            var t = s / steps;
                                                            // evalEasing を root 経由で呼ぶために一時的に selectedType を使えないため
                                                            // ミニプレビュー専用のシンプルな評価
                                                            var y = miniEval(t, etype);
                                                            var px = t * w;
                                                            var py = h - Math.max(-0.3, Math.min(1.3, y)) * h;
                                                            s === 0 ? ctx.moveTo(px, py) : ctx.lineTo(px, py);
                                                        }
                                                        ctx.stroke();
                                                    }

                                                    Connections {
                                                        function onSelectedTypeChanged() {
                                                            miniCanvas.requestPaint();
                                                        }

                                                        target: root
                                                    }

                                                }

                                                Label {
                                                    text: {
                                                        if (easingName === "linear")
                                                            return qsTr("直線");

                                                        if (easingName === "custom")
                                                            return qsTr("カスタム");

                                                        if (easingName.indexOf("_in_out_") !== -1)
                                                            return qsTr("加減速");

                                                        if (easingName.indexOf("_out_in_") !== -1)
                                                            return qsTr("減加速");

                                                        if (easingName.indexOf("_in_") !== -1)
                                                            return qsTr("加速");

                                                        if (easingName.indexOf("_out_") !== -1)
                                                            return qsTr("減速");

                                                        return easingName;
                                                    }
                                                    font.pixelSize: 9
                                                    color: isCurrent ? "white" : palette.mid
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                    horizontalAlignment: Text.AlignHCenter
                                                }

                                            }

                                            background: Rectangle {
                                                color: isCurrent ? palette.highlight : (parent.hovered ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(1, 1, 1, 0.03))
                                                border.color: isCurrent ? palette.highlight : palette.mid
                                                border.width: isCurrent ? 2 : 1
                                                radius: 4
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

    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 8

            Label {
                text: qsTr("補間方法:")
                font.pixelSize: 12
            }

            ComboBox {
                id: displayTypeCombo

                font.pixelSize: 12
                Layout.preferredWidth: 150
                textRole: "text"
                valueRole: "value"
                model: [{
                    "text": qsTr("瞬間移動"),
                    "value": "none"
                }, {
                    "text": qsTr("直線移動"),
                    "value": "linear"
                }, {
                    "text": qsTr("曲線移動"),
                    "value": "custom"
                }, {
                    "text": qsTr("ランダム移動"),
                    "value": "random"
                }, {
                    "text": qsTr("反復移動"),
                    "value": "alternate"
                }]
                currentIndex: {
                    for (var i = 0; i < count; i++) {
                        if (model[i].value === root.selectedType)
                            return i;

                    }
                    return -1;
                }
                onActivated: {
                    if (currentIndex >= 0)
                        root.selectedType = model[currentIndex].value;

                }
            }

            RowLayout {
                visible: root.selectedType === "random" || root.selectedType === "alternate"
                spacing: 4

                Label {
                    text: qsTr("更新間隔:")
                    font.pixelSize: 12
                }

                SpinBox {
                    from: 1
                    to: 9999
                    value: root.stepFrames
                    onValueModified: root.stepFrames = value
                    font.pixelSize: 12
                }

                Label {
                    text: qsTr("フレーム")
                    font.pixelSize: 12
                }

            }

            // spacer
            Item {
                Layout.fillWidth: true
            }

        }

    }

}
