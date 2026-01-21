import QtQuick 2.15
import QtQuick3D 6.0

Node {
    id: root
    // 親から受け取るサイズプロパティ (初期値100)
    property real sizeW: 100
    property real sizeH: 100
    property color color: "#66aa99"
    property real opacity: 1.0

    Model {
        source: "#Rectangle"
        // 100x100 のプリミティブを目的のピクセルサイズに合わせる
        scale: Qt.vector3d(root.sizeW / 100, root.sizeH / 100, 1)
        opacity: root.opacity
        materials: DefaultMaterial { diffuseColor: root.color; lighting: DefaultMaterial.NoLighting }
    }
}
