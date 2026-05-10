#include "filament_canvas.hpp"
#include <QDebug>
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/SwapChain.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <utils/EntityManager.h>

#if defined(Q_OS_MACOS)
#include <CoreVideo/CoreVideo.h>
#include <QuartzCore/CAMetalLayer.h>
#endif

// Wayland 型の前方宣言。
// wayland-client-core.h は CMake の include パスに含まれない場合があるため、
// ヘッダに依存せず opaque pointer として扱う。
// Filament の Vulkan バックエンドが要求する構造体レイアウトは
//   { void* display, void* surface } の順であることが仕様で保証されている。
#if defined(Q_OS_LINUX)
struct wl_display;
struct wl_surface;

struct FilamentWaylandNative {
    wl_display *display = nullptr;
    wl_surface *surface = nullptr;
};
#endif

namespace AviQtl::Rendering {

FilamentCanvas::FilamentCanvas(QQuickItem *parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    connect(this, &QQuickItem::windowChanged, this, &FilamentCanvas::handleWindowChanged);
}

FilamentCanvas::~FilamentCanvas() { destroyFilament(); }

void FilamentCanvas::setSceneId(int id) {
    if (m_sceneId == id)
        return;
    m_sceneId = id;
    emit sceneIdChanged(id);
}

void FilamentCanvas::setCurrentFrame(int frame) {
    if (m_currentFrame == frame)
        return;
    m_currentFrame = frame;
    emit currentFrameChanged(frame);
    update();
}

void FilamentCanvas::handleWindowChanged(QQuickWindow *win) {
    if (win) {
        m_window = win;
        m_beforeRenderingConnection = connect(win, &QQuickWindow::beforeRendering, this, &FilamentCanvas::renderFrame, Qt::DirectConnection);
        m_sceneGraphInvalidatedConnection = connect(win, &QQuickWindow::sceneGraphInvalidated, this, &FilamentCanvas::destroyFilament, Qt::DirectConnection);
    }
}

void FilamentCanvas::initFilament() {
    if (m_engine || !m_window)
        return;

    if (!m_window->isExposed())
        return;

    void *swapChainNative = nullptr;

#if defined(Q_OS_LINUX)
    QPlatformNativeInterface *ni = QGuiApplication::platformNativeInterface();
    if (!ni) {
        qWarning() << "[FilamentCanvas] QPlatformNativeInterface unavailable.";
        return;
    }

    // QPlatformNativeInterface は void* を返すため、
    // wayland-client-core.h のインクルードなしに前方宣言型へキャストできる。
    auto *wlSurface = static_cast<wl_surface *>(ni->nativeResourceForWindow(QByteArrayLiteral("surface"), m_window));
    auto *wlDisplay = static_cast<wl_display *>(ni->nativeResourceForIntegration(QByteArrayLiteral("wl_display")));

    if (wlSurface && wlDisplay) {
        // Filament Vulkan バックエンドは wl_surface* 単体ではなく
        // { wl_display*, wl_surface* } の構造体ポインタを要求する。
        // wl_surface* 単体を渡すと "enumerate size error" でクラッシュする。
        // この構造体は SwapChain が破棄されるまで生存させる必要があるため
        // メンバ変数 m_waylandNative として保持する。
        m_waylandNative = std::make_unique<FilamentWaylandNative>();
        m_waylandNative->display = wlDisplay;
        m_waylandNative->surface = wlSurface;
        swapChainNative = m_waylandNative.get();
        qDebug() << "[FilamentCanvas] Wayland: display=" << wlDisplay << "surface=" << wlSurface;
    } else {
        // X11 フォールバック
        void *x11Handle = ni->nativeResourceForWindow(QByteArrayLiteral("handle"), m_window);
        if (x11Handle) {
            swapChainNative = x11Handle;
            qDebug() << "[FilamentCanvas] X11: handle=" << x11Handle;
        }
    }
#elif defined(Q_OS_WIN)
    if (QPlatformNativeInterface *ni = QGuiApplication::platformNativeInterface()) {
        swapChainNative = ni->nativeResourceForWindow(QByteArrayLiteral("handle"), m_window);
    }
#endif

    // 最終フォールバック
    if (!swapChainNative) {
        swapChainNative = reinterpret_cast<void *>(m_window->winId());
    }

    if (!swapChainNative) {
        qWarning() << "[FilamentCanvas] Failed to obtain native window handle.";
        return;
    }

    qDebug() << "[FilamentCanvas] Native bridge established. SwapChain ptr:" << swapChainNative;

#if defined(Q_OS_MACOS)
    m_engine = filament::Engine::create(filament::Engine::Backend::METAL);
#else
    m_engine = filament::Engine::create(filament::Engine::Backend::VULKAN);
#endif

    if (!m_engine)
        return;

    m_swapChain = m_engine->createSwapChain(swapChainNative);
    m_renderer = m_engine->createRenderer();
    m_scene = m_engine->createScene();
    m_view = m_engine->createView();

    m_cameraEntity = utils::EntityManager::get().create();
    m_camera = m_engine->createCamera(m_cameraEntity);

    m_view->setScene(m_scene);
    m_view->setCamera(m_camera);

    m_view->setPostProcessingEnabled(false);

    // 仕様書に基づき、クリアカラーを紺色 (#001A33 相当) に設定
    m_skybox = filament::Skybox::Builder().color({0.0f, 0.1f, 0.2f, 1.0f}).build(*m_engine);
    m_scene->setSkybox(m_skybox);

    updateViewport(width(), height());
}

void FilamentCanvas::destroyFilament() {
    if (!m_engine)
        return;

    if (m_camera) {
        m_engine->destroyCameraComponent(m_cameraEntity);
        utils::EntityManager::get().destroy(m_cameraEntity);
        m_camera = nullptr;
    }

    m_engine->destroy(m_skybox);
    m_engine->destroy(m_renderer);
    m_engine->destroy(m_view);
    m_engine->destroy(m_scene);
    m_engine->destroy(m_swapChain);

    filament::Engine::destroy(&m_engine);
    m_engine = nullptr;

#if defined(Q_OS_LINUX)
    m_waylandNative.reset();
#endif
}

void FilamentCanvas::renderFrame() {
    if (!m_engine) {
        initFilament();
    }

    if (m_renderer && m_swapChain && m_view) {
        if (m_renderer->beginFrame(m_swapChain)) {
            m_renderer->render(m_view);
            m_renderer->endFrame();
        }
    }
}

void FilamentCanvas::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    updateViewport(newGeometry.width(), newGeometry.height());
#if defined(Q_OS_MACOS)
    updateNativeSurfaceGeometry();
#endif
}

void FilamentCanvas::updateViewport(int w, int h) {
    if (!m_engine || !m_view || !m_camera || w <= 0 || h <= 0)
        return;

    const double dpr = m_window ? m_window->devicePixelRatio() : 1.0;
    const uint32_t pw = static_cast<uint32_t>(w * dpr);
    const uint32_t ph = static_cast<uint32_t>(h * dpr);

    m_view->setViewport({0, 0, pw, ph});

    // AviUtl互換の正投影 (Z=0をピクセル等倍)
    m_camera->setProjection(filament::Camera::Projection::ORTHO, 0, (double)w, (double)h, 0, -1.0, 1.0);
}

void FilamentCanvas::updateNativeSurfaceGeometry() {
#if defined(Q_OS_MACOS)
    if (m_nativeSurface) {
        // Metal Layer のリサイズ同期
    }
#endif
}

void FilamentCanvas::itemChange(ItemChange change, const ItemChangeData &value) {
    QQuickItem::itemChange(change, value);
    if (change == ItemVisibleHasChanged && !value.boolValue) {
        // 非表示時の最適化が必要ならここに記述
    }
}

} // namespace AviQtl::Rendering
