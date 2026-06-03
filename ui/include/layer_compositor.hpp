#pragma once
#include "../../core/include/effect_model.hpp"
#include "timeline_service.hpp"
#include "timeline_types.hpp"
#include <QMatrix4x4>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QVariant>
#include <vector>

namespace AviQtl::UI {

struct ClipTransform {
    double x = 0;
    double y = 0;
    double z = 0;
    double rotX = 0;
    double rotY = 0;
    double rotZ = 0;
    double scaleX = 1;
    double scaleY = 1;
    double scaleZ = 1;
    double opacity = 1;
    double anchorX = 0;
    double anchorY = 0;
    double anchorZ = 0;
};

struct GroupState {
    int clipLayer = -1;
    int layerCount = 1;
    double x = 0, y = 0, z = 0;
    double rotX = 0, rotY = 0, rotZ = 0;
    double scale = 100;
    double opacity = 1;
};

struct CameraState {
    int clipLayer = INT_MAX;
    QPointer<QObject> cameraObject;
};

class LayerCompositor : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(AviQtl::UI::TimelineService *timelineService READ timelineService WRITE setTimelineService NOTIFY timelineServiceChanged)
    Q_PROPERTY(int currentFrame READ currentFrame WRITE setCurrentFrame NOTIFY currentFrameChanged)
    Q_PROPERTY(int currentSceneId READ currentSceneId WRITE setCurrentSceneId NOTIFY currentSceneIdChanged)
    Q_PROPERTY(double projectFps READ projectFps WRITE setProjectFps NOTIFY projectFpsChanged)
    Q_PROPERTY(QObject *activeCameraObject READ activeCameraObject NOTIFY activeCameraObjectChanged)

  public:
    explicit LayerCompositor(QObject *parent = nullptr);

    TimelineService *timelineService() const { return m_timeline; }
    void setTimelineService(TimelineService *svc);

    int currentFrame() const { return m_frame; }
    void setCurrentFrame(int frame);

    int currentSceneId() const { return m_sceneId; }
    void setCurrentSceneId(int id);

    double projectFps() const { return m_fps; }
    void setProjectFps(double fps);

    QObject *activeCameraObject() const;

    Q_INVOKABLE void registerGroupControl(int clipLayer, int layerCount, double x, double y, double z, double rotX, double rotY, double rotZ, double scale, double opacity);
    Q_INVOKABLE void unregisterGroupControl(int clipLayer);
    Q_INVOKABLE void updateGroupControl(int clipLayer, int layerCount, double x, double y, double z, double rotX, double rotY, double rotZ, double scale, double opacity);
    Q_INVOKABLE void registerCameraControl(int clipLayer, QObject *cameraObj);
    Q_INVOKABLE void unregisterCameraControl(int clipLayer);
    Q_INVOKABLE QVariantMap clipTransform(int clipId) const;
    Q_INVOKABLE bool isClipActive(int clipId) const;
    Q_INVOKABLE QQuickItem *findClipMaskSource(int clipId, int clipLayer, int sceneId, const QVariantList &allNodes) const;

  signals:
    void timelineServiceChanged();
    void currentFrameChanged();
    void currentSceneIdChanged();
    void projectFpsChanged();
    void activeCameraObjectChanged();
    void groupControlsChanged();
    void cameraControlsChanged();

  private:
    ClipTransform computeTransform(const ClipData &clip) const;
    const EffectModel *findTransformModel(const ClipData &clip) const;
    void applyGroups(int clipLayer, double &x, double &y, double &z, double &rotX, double &rotY, double &rotZ, double &opacity) const;

    TimelineService *m_timeline = nullptr;
    int m_frame = 0;
    int m_sceneId = -1;
    double m_fps = 60.0;
    std::vector<GroupState> m_groups;
    std::vector<CameraState> m_cameras;
};

} // namespace AviQtl::UI
