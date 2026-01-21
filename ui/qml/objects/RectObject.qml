import QtQuick 2.15
import QtQuick3D 6.0

Node {
    id: root

    Model {
        source: "#Rectangle"
        scale: Qt.vector3d(3, 2, 1)
        materials: DefaultMaterial {
            diffuseColor: "#66aa99"
            lighting: DefaultMaterial.NoLighting
        }
    }
}
