#include "layer_compositor.hpp"
#include <QMatrix4x4>
#include <algorithm>
#include <cmath>
#include <limits>

namespace AviQtl::UI {

LayerCompositor::LayerCompositor(QObject *parent) : QObject(parent) {}

void LayerCompositor::setTimelineService(TimelineService *svc) {
    if (m_timeline == svc)
        return;
    m_timeline = svc;
    emit timelineServiceChanged();
}

void LayerCompositor::setCurrentFrame(int frame) {
    if (m_frame == frame)
        return;
    m_frame = frame;
    emit currentFrameChanged();
}

void LayerCompositor::setCurrentSceneId(int id) {
    if (m_sceneId == id)
        return;
    m_sceneId = id;
    emit currentSceneIdChanged();
}

void LayerCompositor::setProjectFps(double fps) {
    if (qFuzzyCompare(m_fps, fps))
        return;
    m_fps = fps;
    emit projectFpsChanged();
}

void LayerCompositor::registerGroupControl(int clipLayer, int layerCount, double x, double y, double z, double rotX, double rotY, double rotZ, double scale, double opacity) {
    for (auto &g : m_groups) {
        if (g.clipLayer == clipLayer) {
            g = {clipLayer, layerCount, x, y, z, rotX, rotY, rotZ, scale, opacity};
            emit groupControlsChanged();
            return;
        }
    }
    m_groups.push_back({clipLayer, layerCount, x, y, z, rotX, rotY, rotZ, scale, opacity});
    std::sort(m_groups.begin(), m_groups.end(), [](const auto &a, const auto &b) { return a.clipLayer < b.clipLayer; });
    emit groupControlsChanged();
}

void LayerCompositor::updateGroupControl(int clipLayer, int layerCount, double x, double y, double z, double rotX, double rotY, double rotZ, double scale, double opacity) {
    registerGroupControl(clipLayer, layerCount, x, y, z, rotX, rotY, rotZ, scale, opacity);
}

void LayerCompositor::unregisterGroupControl(int clipLayer) {
    auto it = std::remove_if(m_groups.begin(), m_groups.end(), [clipLayer](const auto &g) { return g.clipLayer == clipLayer; });
    if (it != m_groups.end()) {
        m_groups.erase(it, m_groups.end());
        emit groupControlsChanged();
    }
}

void LayerCompositor::registerCameraControl(int clipLayer, QObject *cameraObj) {
    for (auto &c : m_cameras) {
        if (c.clipLayer == clipLayer) {
            c.cameraObject = QPointer<QObject>(cameraObj);
            emit cameraControlsChanged();
            emit activeCameraObjectChanged();
            return;
        }
    }
    m_cameras.push_back({clipLayer, QPointer<QObject>(cameraObj)});
    std::sort(m_cameras.begin(), m_cameras.end(), [](const auto &a, const auto &b) { return a.clipLayer < b.clipLayer; });
    emit cameraControlsChanged();
    emit activeCameraObjectChanged();
}

void LayerCompositor::unregisterCameraControl(int clipLayer) {
    auto it = std::remove_if(m_cameras.begin(), m_cameras.end(), [clipLayer](const auto &c) { return c.clipLayer == clipLayer; });
    if (it != m_cameras.end()) {
        m_cameras.erase(it, m_cameras.end());
        emit cameraControlsChanged();
        emit activeCameraObjectChanged();
    }
}

QObject *LayerCompositor::activeCameraObject() const {
    for (const auto &c : m_cameras) {
        if (!c.cameraObject.isNull())
            return c.cameraObject.data();
    }
    return nullptr;
}

const EffectModel *LayerCompositor::findTransformModel(const ClipData &clip) const {
    for (const EffectModel *ef : clip.effects) {
        if (ef && ef->id() == QStringLiteral("transform"))
            return ef;
    }
    return nullptr;
}

void LayerCompositor::applyGroups(int clipLayer, double &x, double &y, double &z, double &rotX, double &rotY, double &rotZ, double &opacity) const {
    QMatrix4x4 m;
    for (const GroupState &g : m_groups) {
        if (g.clipLayer >= clipLayer || g.clipLayer + g.layerCount < clipLayer)
            continue;
        m.translate(g.x, g.y, g.z);
        m.rotate(g.rotX, 1, 0, 0);
        m.rotate(g.rotY, 0, 1, 0);
        m.rotate(g.rotZ, 0, 0, 1);
        const float s = static_cast<float>(g.scale / 100.0);
        m.scale(s, s, s);
        opacity *= g.opacity;
    }
    const QVector3D p = m.map(QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
    x = p.x();
    y = p.y();
    z = p.z();
    for (const GroupState &g : m_groups) {
        if (g.clipLayer >= clipLayer || g.clipLayer + g.layerCount < clipLayer)
            continue;
        rotX += g.rotX;
        rotY += g.rotY;
        rotZ += g.rotZ;
    }
}

ClipTransform LayerCompositor::computeTransform(const ClipData &clip) const {
    ClipTransform out;
    if (const auto *tm = findTransformModel(clip)) {
        out.x = tm->evaluatedParam(QStringLiteral("x"), m_frame, m_fps).toDouble();
        out.y = tm->evaluatedParam(QStringLiteral("y"), m_frame, m_fps).toDouble();
        out.z = tm->evaluatedParam(QStringLiteral("z"), m_frame, m_fps).toDouble();
        out.rotX = tm->evaluatedParam(QStringLiteral("rotationX"), m_frame, m_fps).toDouble();
        out.rotY = tm->evaluatedParam(QStringLiteral("rotationY"), m_frame, m_fps).toDouble();
        out.rotZ = tm->evaluatedParam(QStringLiteral("rotationZ"), m_frame, m_fps).toDouble();
        const double scale = tm->evaluatedParam(QStringLiteral("scale"), m_frame, m_fps).toDouble() / 100.0;
        out.scaleX = scale;
        out.scaleY = scale;
        out.scaleZ = scale;
        out.opacity = tm->evaluatedParam(QStringLiteral("opacity"), m_frame, m_fps).toDouble();
        out.anchorX = tm->evaluatedParam(QStringLiteral("anchorX"), m_frame, m_fps).toDouble();
        out.anchorY = tm->evaluatedParam(QStringLiteral("anchorY"), m_frame, m_fps).toDouble();
        out.anchorZ = tm->evaluatedParam(QStringLiteral("anchorZ"), m_frame, m_fps).toDouble();
    }
    applyGroups(clip.layer, out.x, out.y, out.z, out.rotX, out.rotY, out.rotZ, out.opacity);
    return out;
}

QVariantMap LayerCompositor::clipTransform(int clipId) const {
    QVariantMap m;
    if (!m_timeline)
        return m;
    const auto *clip = m_timeline->findClipById(clipId);
    if (!clip)
        return m;
    const auto tf = computeTransform(*clip);
    m[QStringLiteral("x")] = tf.x;
    m[QStringLiteral("y")] = tf.y;
    m[QStringLiteral("z")] = tf.z;
    m[QStringLiteral("rx")] = tf.rotX;
    m[QStringLiteral("ry")] = tf.rotY;
    m[QStringLiteral("rz")] = tf.rotZ;
    m[QStringLiteral("sx")] = tf.scaleX;
    m[QStringLiteral("sy")] = tf.scaleY;
    m[QStringLiteral("sz")] = tf.scaleZ;
    m[QStringLiteral("opacity")] = tf.opacity;
    m[QStringLiteral("anchorX")] = tf.anchorX;
    m[QStringLiteral("anchorY")] = tf.anchorY;
    m[QStringLiteral("anchorZ")] = tf.anchorZ;
    return m;
}

bool LayerCompositor::isClipActive(int clipId) const {
    if (!m_timeline)
        return false;
    const auto *clip = m_timeline->findClipById(clipId);
    if (!clip)
        return false;
    return m_frame >= clip->startFrame && m_frame < (clip->startFrame + clip->durationFrames);
}

QQuickItem *LayerCompositor::findClipMaskSource(int clipId, int clipLayer, int sceneId, const QVariantList &allNodes) const {
    if (!m_timeline)
        return nullptr;
    const auto *thisClip = m_timeline->findClipById(clipId);
    if (!thisClip)
        return nullptr;
    int nearestLayer = std::numeric_limits<int>::min();
    QQuickItem *nearestItem = nullptr;
    for (const QVariant &v : allNodes) {
        QObject *obj = v.value<QObject *>();
        if (!obj || obj->property("visible").toBool() == false)
            continue;
        const int candLayer = obj->property("clipLayerRole").toInt();
        const int candScene = obj->property("clipSceneIdRole").toInt();
        if (candScene != -1 && sceneId != -1 && candScene != sceneId)
            continue;
        if (candLayer <= clipLayer && candLayer > nearestLayer) {
            auto *item = qobject_cast<QQuickItem *>(obj->property("rawFbRendererOutput").value<QObject *>());
            if (item) {
                nearestLayer = candLayer;
                nearestItem = item;
            }
        }
    }
    return nearestItem;
}

} // namespace AviQtl::UI
