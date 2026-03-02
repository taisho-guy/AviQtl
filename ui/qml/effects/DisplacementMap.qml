import QtQuick
import "qrc:/qt/qml/Rina/ui/qml/common" as Common

Common.BaseEffect {
    id: root

    // AviUtl: 変形X/Y/拡大変形/拡大縦横/回転変形
    property real deformX: root.evalNumber("deformX", 0)
    property real deformY: root.evalNumber("deformY", 0)
    property real deformScale: root.evalNumber("deformScale", 50)
    property real deformAspect: root.evalNumber("deformAspect", 100)
    property real deformRotation: root.evalNumber("deformRotation", 0)
    // AviUtl: X/Y/回転/サイズ/縦横比/ぼかし
    property real centerX: root.evalNumber("x", 0)
    property real centerY: root.evalNumber("y", 0)
    property real shapeRot: root.evalNumber("rotation", 0)
    property real shapeSize: Math.max(1, root.evalNumber("size", 100))
    property real shapeAspect: Math.max(0.01, root.evalNumber("aspect", 100))
    property real shapeBlur: Math.max(0.5, root.evalNumber("blur", 20))
    // AviUtl: マップの種類 (円/四角形/三角形/五角形/星)
    property real mapType: {
        var m = root.evalParam("mapType", "circle");
        if (m === "square")
            return 1;

        if (m === "triangle")
            return 2;

        if (m === "pentagon")
            return 3;

        if (m === "star")
            return 4;

        return 0;
    }
    // AviUtl: 変形方法 (移動変形/拡大変形/回転変形)
    property real deformMethod: {
        var d = root.evalParam("deformMethod", "move");
        if (d === "scale")
            return 1;

        if (d === "rotate")
            return 2;

        return 0;
    }

    ShaderEffect {
        property variant source: root.sourceProxy
        property real deformX: root.deformX
        property real deformY: root.deformY
        property real deformScale: root.deformScale
        property real deformAspect: root.deformAspect
        property real deformRotation: root.deformRotation
        property real centerX: root.centerX
        property real centerY: root.centerY
        property real shapeRot: root.shapeRot
        property real shapeSize: root.shapeSize
        property real shapeAspect: root.shapeAspect
        property real shapeBlur: root.shapeBlur
        property real mapType: root.mapType
        property real deformMethod: root.deformMethod
        property real targetWidth: root.width
        property real targetHeight: root.height

        anchors.fill: parent
        fragmentShader: "displacement.frag.qsb"
    }

}
