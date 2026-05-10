#pragma once

#include <QMetaObject>
#include <QQuickItem>
#include <utils/Entity.h>

#include <memory>

// Wayland 型の前方宣言 (wayland-client-core.h 不要)
#if defined(Q_OS_LINUX)
struct FilamentWaylandNative;
#endif

namespace filament {
class Engine;
class Renderer;
class Scene;
class View;
class Camera;
class SwapChain;
class Skybox;
} // namespace filament

namespace AviQtl::Rendering {

// QMLから "FilamentCanvas" として利用可能なFilament統合ウィジェット。
// Filamentの初期化・描画ループ・リサイズをカプセル化し、
// 外部(QML/ECS)にはsceneId/currentFrameプロパティのみを公開する。
// 注: QML登録は main.cpp で qmlRegisterType により手動実施する。
class FilamentCanvas : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(int sceneId READ sceneId WRITE setSceneId NOTIFY sceneIdChanged)
    Q_PROPERTY(int currentFrame READ currentFrame WRITE setCurrentFrame NOTIFY currentFrameChanged)

  public:
    explicit FilamentCanvas(QQuickItem *parent = nullptr);
    ~FilamentCanvas() override;

    int sceneId() const { return m_sceneId; }
    void setSceneId(int id);

    int currentFrame() const { return m_currentFrame; }
    void setCurrentFrame(int frame);

  signals:
    void sceneIdChanged(int id);
    void currentFrameChanged(int frame);

  protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

  private slots:
    void handleWindowChanged(QQuickWindow *win);
    void renderFrame();
    void destroyFilament();

  private:
    void initFilament();
    void updateViewport(int w, int h);
    void updateNativeSurfaceGeometry();

    int m_sceneId = -1;
    int m_currentFrame = 0;

    QQuickWindow *m_window = nullptr;
    QMetaObject::Connection m_beforeRenderingConnection;
    QMetaObject::Connection m_sceneGraphInvalidatedConnection;

    // Filament objects (Forward declared)
    filament::Engine *m_engine = nullptr;
    filament::Renderer *m_renderer = nullptr;
    filament::Scene *m_scene = nullptr;
    filament::View *m_view = nullptr;
    filament::Camera *m_camera = nullptr;
    filament::SwapChain *m_swapChain = nullptr;
    filament::Skybox *m_skybox = nullptr;

    // Utils::Entity handles
    utils::Entity m_cameraEntity;

    // Native surface pointer (e.g. CAMetalLayer* for Apple/Metal)
    void *m_nativeSurface = nullptr;

#if defined(Q_OS_LINUX)
    // Wayland: { wl_display*, wl_surface* } の組を SwapChain 生存期間中保持する。
    // wl_surface* 単体を Filament に渡すと "enumerate size error" でクラッシュする。
    // unique_ptr で管理し、wayland-client-core.h への依存をヘッダから排除する。
    std::unique_ptr<FilamentWaylandNative> m_waylandNative;
#endif
};

} // namespace AviQtl::Rendering
