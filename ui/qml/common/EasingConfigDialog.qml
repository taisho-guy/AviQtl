import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property var effectModel: null
    property string paramName: ""
    property int keyframeFrame: 0
    property string selectedType: "linear"
    // [cp1x, cp1y, cp2x, cp2y, endX, endY, ...]
    property var bezierParams: [0.33, 0, 0.66, 1, 1, 1]

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
                // 旧データ後方互換: bezier → custom へ昇格
                if (selectedType === "bezier")
                    selectedType = "custom";

                var idx = typeCombo.model.indexOf(selectedType);
                typeCombo.currentIndex = idx >= 0 ? idx : 0;
                if (selectedType === "custom") {
                    if (kf.points && kf.points.length >= 6) {
                        var pts = [];
                        for (let j = 0; j < kf.points.length; ++j) pts.push(kf.points[j])
                        bezierParams = pts;
                    } else {
                        // 旧 bezier 形式を custom 6要素形式に変換
                        bezierParams = [kf.bzx1 !== undefined ? kf.bzx1 : 0.33, kf.bzy1 !== undefined ? kf.bzy1 : 0, kf.bzx2 !== undefined ? kf.bzx2 : 0.66, kf.bzy2 !== undefined ? kf.bzy2 : 1, 1, 1];
                    }
                }
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
        if (selectedType === "custom") {
            var pts = [];
            for (let j = 0; j < bezierParams.length; ++j) pts.push(bezierParams[j])
            options.points = pts;
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

            Item {
                implicitWidth: 320
                implicitHeight: 160
                Layout.fillWidth: true

                Canvas {
                    id: previewCanvas

                    // ハンドルのピクセル座標を返す [[px,py], ...]
                    function handlePositions() {
                        var bz = root.bezierParams;
                        var w = width, h = height;
                        var result = [];
                        var prevX = 0, prevY = 0;
                        for (var i = 0; i < bz.length; i += 6) {
                            result.push([bz[i] * w, (1 - bz[i + 1]) * h]); // cp1
                            result.push([bz[i + 2] * w, (1 - bz[i + 3]) * h]); // cp2
                            // 最終セグメントでなければ anchorPoint も移動可能
                            if (i + 6 < bz.length)
                                result.push([bz[i + 4] * w, (1 - bz[i + 5]) * h]);

                            // anchor
                            prevX = bz[i + 4];
                            prevY = bz[i + 5];
                        }
                        return result;
                    }

                    anchors.fill: parent
                    onPaint: {
                        var ctx = getContext("2d");
                        var w = width, h = height;
                        ctx.clearRect(0, 0, w, h);
                        // 背景
                        ctx.fillStyle = "#222";
                        ctx.fillRect(0, 0, w, h);
                        // グリッド
                        ctx.strokeStyle = "#444";
                        ctx.lineWidth = 0.5;
                        for (var i = 1; i < 4; i++) {
                            ctx.beginPath();
                            ctx.moveTo(w * (i / 4), 0);
                            ctx.lineTo(w * (i / 4), h);
                            ctx.stroke();
                            ctx.beginPath();
                            ctx.moveTo(0, h * (i / 4));
                            ctx.lineTo(w, h * (i / 4));
                            ctx.stroke();
                        }
                        // 対角線（linear 参照）
                        ctx.strokeStyle = "#555";
                        ctx.lineWidth = 1;
                        ctx.beginPath();
                        ctx.moveTo(0, h);
                        ctx.lineTo(w, 0);
                        ctx.stroke();
                        var bz = root.bezierParams;
                        // 青い曲線
                        ctx.strokeStyle = "#48f";
                        ctx.lineWidth = 2;
                        ctx.beginPath();
                        for (var s = 0; s <= 64; s++) {
                            var t = s / 64;
                            var y = evalEasing(t);
                            var px = t * w;
                            var py = h - Math.max(0, Math.min(1, y)) * h;
                            s === 0 ? ctx.moveTo(px, py) : ctx.lineTo(px, py);
                        }
                        ctx.stroke();
                        // custom のときハンドルとタンジェントラインを描画
                        if (root.selectedType === "custom") {
                            var prevAnchorX = 0, prevAnchorY = h;
                            for (var seg = 0; seg < bz.length; seg += 6) {
                                var c1px = bz[seg] * w, c1py = (1 - bz[seg + 1]) * h;
                                var c2px = bz[seg + 2] * w, c2py = (1 - bz[seg + 3]) * h;
                                var enpx = bz[seg + 4] * w, enpy = (1 - bz[seg + 5]) * h;
                                // タンジェントライン
                                ctx.strokeStyle = "#888";
                                ctx.lineWidth = 0.8;
                                ctx.setLineDash([3, 3]);
                                ctx.beginPath();
                                ctx.moveTo(prevAnchorX, prevAnchorY);
                                ctx.lineTo(c1px, c1py);
                                ctx.stroke();
                                ctx.beginPath();
                                ctx.moveTo(enpx, enpy);
                                ctx.lineTo(c2px, c2py);
                                ctx.stroke();
                                ctx.setLineDash([]);
                                // cp1 ハンドル (白丸)
                                ctx.fillStyle = "#fff";
                                ctx.beginPath();
                                ctx.arc(c1px, c1py, 5, 0, 2 * Math.PI);
                                ctx.fill();
                                // cp2 ハンドル (白丸)
                                ctx.fillStyle = "#fff";
                                ctx.beginPath();
                                ctx.arc(c2px, c2py, 5, 0, 2 * Math.PI);
                                ctx.fill();
                                // anchor ハンドル (黄丸、始点/終点以外)
                                if (seg + 6 < bz.length) {
                                    ctx.fillStyle = "#ff0";
                                    ctx.beginPath();
                                    ctx.arc(enpx, enpy, 6, 0, 2 * Math.PI);
                                    ctx.fill();
                                }
                                prevAnchorX = enpx;
                                prevAnchorY = enpy;
                            }
                        }
                    }
                    // evalEasing はここでクロージャとして定義 (bz をキャプチャ)
                    function evalEasing(t) {
                        var type = root.selectedType.toLowerCase().replace(/_/g, "");
                        if (type === "easein")
                            return t * t;

                        if (type === "easeout")
                            return t * (2 - t);

                        if (type === "easeinout")
                            return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;

                        if (type === "custom") {
                            var prevX = 0, prevY = 0;
                            for (var i = 0; i < bz.length; i += 6) {
                                var cp1x = bz[i], cp1y = bz[i + 1];
                                var cp2x = bz[i + 2], cp2y = bz[i + 3];
                                var endX = bz[i + 4], endY = bz[i + 5];
                                if (t <= endX || i + 6 >= bz.length) {
                                    var range = endX - prevX;
                                    if (range < 0.0001)
                                        return endY;

                                    var nx = (t - prevX) / range;
                                    var nc1x = (cp1x - prevX) / range;
                                    var nc2x = (cp2x - prevX) / range;
                                    var T = nx;
                                    for (var k = 0; k < 8; k++) {
                                        var u = 1 - T;
                                        var cx = 3 * u * u * T * nc1x + 3 * u * T * T * nc2x + T * T * T;
                                        var err = cx - nx;
                                        if (Math.abs(err) < 0.0001)
                                            break;

                                        var dcx = 3 * u * u * nc1x + 6 * u * T * (nc2x - nc1x) + 3 * T * T * (1 - nc2x);
                                        if (Math.abs(dcx) < 1e-06)
                                            break;

                                        T -= err / dcx;
                                    }
                                    T = Math.max(0, Math.min(1, T));
                                    var u2 = 1 - T;
                                    return u2 * u2 * u2 * prevY + 3 * u2 * u2 * T * cp1y + 3 * u2 * T * T * cp2y + T * T * T * endY;
                                }
                                prevX = endX;
                                prevY = endY;
                            }
                        }
                        return t;
                    }

                }

                // ドラッグ編集用 MouseArea
                MouseArea {
                    property int dragIdx: -1 // bezierParams内のインデックス
                    property bool isDragAnchor: false

                    function findNearestHandle(mx, my) {
                        var bz = root.bezierParams;
                        var w = previewCanvas.width, h = previewCanvas.height;
                        var best = -1, bestDist = 12 * 12; // 12px ヒット半径
                        var prevX = 0, prevY = 0;
                        for (var seg = 0; seg < bz.length; seg += 6) {
                            // cp1
                            var dx = mx - bz[seg] * w, dy = my - (1 - bz[seg + 1]) * h;
                            if (dx * dx + dy * dy < bestDist) {
                                bestDist = dx * dx + dy * dy;
                                best = seg;
                            }
                            // cp2
                            dx = mx - bz[seg + 2] * w;
                            dy = my - (1 - bz[seg + 3]) * h;
                            if (dx * dx + dy * dy < bestDist) {
                                bestDist = dx * dx + dy * dy;
                                best = seg + 2;
                            }
                            // anchor (中間のみ)
                            if (seg + 6 < bz.length) {
                                dx = mx - bz[seg + 4] * w;
                                dy = my - (1 - bz[seg + 5]) * h;
                                if (dx * dx + dy * dy < bestDist) {
                                    bestDist = dx * dx + dy * dy;
                                    best = seg + 4;
                                }
                            }
                        }
                        return best;
                    }

                    anchors.fill: parent
                    enabled: root.selectedType === "custom"
                    onPressed: function(mouse) {
                        dragIdx = findNearestHandle(mouse.x, mouse.y);
                    }
                    onPositionChanged: function(mouse) {
                        if (dragIdx < 0)
                            return ;

                        var w = previewCanvas.width, h = previewCanvas.height;
                        // 論理座標へ変換
                        var lx = Math.max(0, Math.min(1, mouse.x / w));
                        var ly = Math.max(0, Math.min(1, 1 - mouse.y / h));
                        // slice() で配列をコピーして1要素だけ更新
                        var p = root.bezierParams.slice();
                        p[dragIdx] = lx;
                        p[dragIdx + 1] = ly;
                        root.bezierParams = p;
                    }
                    onReleased: {
                        dragIdx = -1;
                    }
                }

            }

        }

        GroupBox {
            title: "制御点"
            enabled: root.selectedType === "custom"

            GridLayout {
                columns: 4
                columnSpacing: 10

                Label {
                    text: "CP1 X"
                }

                TextField {
                    id: tfCp1x

                    text: root.bezierParams[0].toFixed(2)
                    onEditingFinished: {
                        var p = root.bezierParams.slice();
                        p[0] = Math.max(0, Math.min(1, parseFloat(text) || 0));
                        root.bezierParams = p;
                    }

                    // activeFocus 中は外部バインドを抑制
                    Binding on text {
                        when: !tfCp1x.activeFocus
                        value: root.bezierParams[0].toFixed(3)
                    }

                }

                Label {
                    text: "CP1 Y"
                }

                TextField {
                    id: tfCp1y

                    text: root.bezierParams[1].toFixed(2)
                    onEditingFinished: {
                        var p = root.bezierParams.slice();
                        p[1] = parseFloat(text) || 0;
                        root.bezierParams = p;
                    }

                    Binding on text {
                        when: !tfCp1y.activeFocus
                        value: root.bezierParams[1].toFixed(3)
                    }

                }

                Label {
                    text: "CP2 X"
                }

                TextField {
                    id: tfCp2x

                    text: root.bezierParams[2].toFixed(2)
                    onEditingFinished: {
                        var p = root.bezierParams.slice();
                        p[2] = Math.max(0, Math.min(1, parseFloat(text) || 0));
                        root.bezierParams = p;
                    }

                    Binding on text {
                        when: !tfCp2x.activeFocus
                        value: root.bezierParams[2].toFixed(3)
                    }

                }

                Label {
                    text: "CP2 Y"
                }

                TextField {
                    id: tfCp2y

                    text: root.bezierParams[3].toFixed(2)
                    onEditingFinished: {
                        var p = root.bezierParams.slice();
                        p[3] = parseFloat(text) || 0;
                        root.bezierParams = p;
                    }

                    Binding on text {
                        when: !tfCp2y.activeFocus
                        value: root.bezierParams[3].toFixed(3)
                    }

                }

            }

        }

    }

}
