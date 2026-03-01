import QtQuick
import QtQuick.Shapes
import QtQuick3D
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseObject {
    id: root

    property real sizeW: evalParam("rect", "sizeW", 200)
    property real sizeH: evalParam("rect", "sizeH", 200)
    property int sides: Math.max(3, Math.round(Number(evalParam("rect", "sides", 4))))
    property real cornerRadius: evalParam("rect", "cornerRadius", 0)
    property real innerRadius: evalParam("rect", "innerRadius", 50) // % (0-100)
    property real shapeRotDeg: evalParam("rect", "rotation", 0) // 初期回転(°)
    property color fillColor: evalParam("rect", "color", "#66aa99")
    property bool useGradient: evalParam("rect", "useGradient", false)
    property color gradientColor2: evalParam("rect", "gradientColor2", "#ffffff")
    property int gradientType: evalParam("rect", "gradientType", 0) // 0:Linear, 1:Radial
    property color strokeColor: evalParam("rect", "strokeColor", "#ffffff")
    property real strokeWidth: evalParam("rect", "strokeWidth", 0)
    property real dashLength: evalParam("rect", "dashLength", 0)
    property real dashSpace: evalParam("rect", "dashSpace", 0)
    property real opacity: evalParam("rect", "opacity", 1)
    property string shapeType: String(evalParam("rect", "shapeType", "polygon"))
    // 縁取りが見切れないようにパディングを確保
    readonly property real padding: strokeWidth / 2 + 2

    sourceItem: sourceItem

    Item {
        id: sourceItem

        visible: false
        width: root.sizeW + padding * 2
        height: root.sizeH + padding * 2

        // 描画ホスト
        Item {
            id: shapeHost

            anchors.centerIn: parent
            width: root.sizeW
            height: root.sizeH

            Canvas {
                // 偶数角形は頂点を上に
                // Radial
                // Linear (上から下)

                id: shapeCanvas

                // 親のサイズ(sizeW/H)ではなく、padding込みのsourceItemサイズに合わせる
                width: sourceItem.width
                height: sourceItem.height
                anchors.centerIn: parent
                antialiasing: true
                // パラメーター変化 → 再描画
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    ctx.save();
                    // 中心座標もパディング分ずらす
                    var cx = width / 2;
                    var cy = height / 2;
                    var n = root.sides;
                    // 多角形の初期回転補正: 頂点が上に来るように調整
                    // sides=4(正方形)の場合、デフォルトだと45度回転してひし形に見えるのを防ぐ
                    var baseRot = (root.shapeType === "polygon" || root.shapeType === "star") ? -Math.PI / 2 - (Math.PI / n) * (n % 2 === 0 ? 1 : 0) : -Math.PI / 2;
                    // ユーザー指定の回転を加算
                    var rad0 = baseRot + root.shapeRotDeg * Math.PI / 180;
                    var type = root.shapeType;
                    // 塗りつぶしスタイルの設定
                    if (root.useGradient) {
                        var grad;
                        if (root.gradientType === 1)
                            grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, Math.max(root.sizeW, root.sizeH) / 2);
                        else
                            grad = ctx.createLinearGradient(cx, cy - root.sizeH / 2, cx, cy + root.sizeH / 2);
                        grad.addColorStop(0, root.fillColor);
                        grad.addColorStop(1, root.gradientColor2);
                        ctx.fillStyle = grad;
                    } else {
                        ctx.fillStyle = root.fillColor;
                    }
                    ctx.strokeStyle = root.strokeColor;
                    ctx.lineWidth = root.strokeWidth;
                    ctx.lineJoin = "round";
                    // 破線設定
                    if (root.dashLength > 0 || root.dashSpace > 0)
                        ctx.setLineDash([Math.max(1, root.dashLength), Math.max(0, root.dashSpace)]);
                    else
                        ctx.setLineDash([]);
                    if (type === "pie" || type === "arc" || type === "donut") {
                        // arc / pie / donut: sides を「扇の角度(°)」として使用
                        var arcDeg = Math.min(360, Math.max(1, n));
                        // sidesを角度として転用
                        // 円弧系は -90度(12時方向)基準で回転させる
                        var startAng = (root.shapeRotDeg - 90) * Math.PI / 180;
                        var endAng = startAng + arcDeg * Math.PI / 180;
                        var outerR = Math.min(root.sizeW, root.sizeH) / 2;
                        var innerR = outerR * (root.innerRadius / 100);
                        ctx.beginPath();
                        if (type === "donut") {
                            // 外円 → 内円(逆順)
                            ctx.arc(cx, cy, outerR, startAng, endAng, false);
                            ctx.arc(cx, cy, Math.max(1, innerR), endAng, startAng, true);
                            ctx.closePath();
                        } else if (type === "arc") {
                            ctx.arc(cx, cy, outerR, startAng, endAng, false);
                            if (root.strokeWidth > 0)
                                ctx.stroke();

                            ctx.restore();
                            return ;
                        } else {
                            // pie
                            ctx.moveTo(cx, cy);
                            ctx.arc(cx, cy, outerR, startAng, endAng, false);
                            ctx.closePath();
                        }
                    } else if (type === "star") {
                        // star: 外半径と内径(innerRadius)で星形
                        var outerRS = Math.min(root.sizeW, root.sizeH) / 2;
                        var innerRS = outerRS * (root.innerRadius / 100);
                        var totalPts = n * 2;
                        ctx.beginPath();
                        for (var si = 0; si <= totalPts; si++) {
                            var ang = rad0 + si * Math.PI / n;
                            var r = (si % 2 === 0) ? outerRS : innerRS;
                            var px = cx + Math.cos(ang) * r;
                            var py = cy + Math.sin(ang) * r;
                            si === 0 ? ctx.moveTo(px, py) : ctx.lineTo(px, py);
                        }
                        ctx.closePath();
                    } else {
                        // polygon (デフォルト): 角丸対応多角形
                        var outerRP = Math.min(root.sizeW, root.sizeH) / 2;
                        var cr = Math.min(root.cornerRadius, outerRP);
                        if (cr < 0.5 || n > 32) {
                            // 角丸なし: 純粋な多角形
                            ctx.beginPath();
                            for (var pi = 0; pi < n; pi++) {
                                var a = rad0 + pi * 2 * Math.PI / n;
                                var vx = cx + Math.cos(a) * outerRP;
                                var vy = cy + Math.sin(a) * outerRP;
                                pi === 0 ? ctx.moveTo(vx, vy) : ctx.lineTo(vx, vy);
                            }
                            ctx.closePath();
                        } else {
                            // 角丸付き多角形
                            // 各頂点に対して、隣接辺をcrだけ後退した点でarcToを使う
                            ctx.beginPath();
                            var verts = [];
                            for (var vi = 0; vi < n; vi++) {
                                var va = rad0 + vi * 2 * Math.PI / n;
                                verts.push([cx + Math.cos(va) * outerRP, cy + Math.sin(va) * outerRP]);
                            }
                            for (var ki = 0; ki < n; ki++) {
                                var prev = verts[(ki - 1 + n) % n];
                                var curr = verts[ki];
                                var next = verts[(ki + 1) % n];
                                // prev→curr 方向の単位ベクトル
                                var dx1 = curr[0] - prev[0], dy1 = curr[1] - prev[1];
                                var len1 = Math.sqrt(dx1 * dx1 + dy1 * dy1);
                                var dx2 = next[0] - curr[0], dy2 = next[1] - curr[1];
                                var len2 = Math.sqrt(dx2 * dx2 + dy2 * dy2);
                                var r2 = Math.min(cr, len1 / 2, len2 / 2);
                                // 入線終点 (arcToの第1制御点の手前)
                                var ax = curr[0] - dx1 / len1 * r2;
                                var ay = curr[1] - dy1 / len1 * r2;
                                ki === 0 ? ctx.moveTo(ax, ay) : ctx.lineTo(ax, ay);
                                ctx.arcTo(curr[0], curr[1], curr[0] + dx2 / len2 * r2, curr[1] + dy2 / len2 * r2, r2);
                            }
                            ctx.closePath();
                        }
                    }
                    ctx.fill();
                    if (root.strokeWidth > 0)
                        ctx.stroke();

                    ctx.restore();
                }

                // 全パラメーター監視して再描画
                Connections {
                    function onSizeWChanged() {
                        shapeCanvas.requestPaint();
                    }

                    function onSizeHChanged() {
                        shapeCanvas.requestPaint();
                    }

                    function onSidesChanged() {
                        shapeCanvas.requestPaint();
                    }

                    function onCornerRadiusChanged() {
                        shapeCanvas.requestPaint();
                    }

                    function onInnerRadiusChanged() {
                        shapeCanvas.requestPaint();
                    }

                    function onShapeRotDegChanged() {
                        shapeCanvas.requestPaint();
                    }

                    function onFillColorChanged() {
                        shapeCanvas.requestPaint();
                    }

                    function onUseGradientChanged() {
                        shapeCanvas.requestPaint();
                    }

                    function onGradientColor2Changed() {
                        shapeCanvas.requestPaint();
                    }

                    function onGradientTypeChanged() {
                        shapeCanvas.requestPaint();
                    }

                    function onStrokeColorChanged() {
                        shapeCanvas.requestPaint();
                    }

                    function onStrokeWidthChanged() {
                        shapeCanvas.requestPaint();
                    }

                    function onDashLengthChanged() {
                        shapeCanvas.requestPaint();
                    }

                    function onDashSpaceChanged() {
                        shapeCanvas.requestPaint();
                    }

                    function onShapeTypeChanged() {
                        shapeCanvas.requestPaint();
                    }

                    target: root
                }

            }

        }

    }

    Model {
        source: "#Rectangle"
        scale: Qt.vector3d((renderer.output.sourceItem ? renderer.output.sourceItem.width : sourceItem.width) / 100, (renderer.output.sourceItem ? renderer.output.sourceItem.height : sourceItem.height) / 100, 1)
        opacity: root.opacity

        materials: DefaultMaterial {
            lighting: DefaultMaterial.NoLighting
            blendMode: root.blendMode
            cullMode: root.cullMode

            diffuseMap: Texture {
                sourceItem: renderer.output
            }

        }

    }

}
