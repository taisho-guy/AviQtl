#include "filament_canvas.hpp"

#include <QDebug>
#include <QQuickWindow>
#include <QRhi>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

// Qt 6.8+ Vulkan ネイティブテクスチャ API
#include <QNativeInterface>
#include <rhi/qrhi.h>

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/RenderTarget.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/SwapChain.h>
#include <filament/Texture.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <utils/EntityManager.h>

// ─────────────────────────────────────────────────────────────────────────────
// 設計メモ (フェーズ2):
//
//  VulkanSharedContextQt アーキテクチャ:
//    QtVulkanPlatform が QQuickWindow の QRhi (Vulkan バックエンド) から
//    VkInstance / VkPhysicalDevice / VkDevice / queueFamilyIndex を取得し、
//    Filament::Engine に同一デバイスを渡す。
//
//    描画フロー:
//      1. QtVulkanPlatform::createSwapChain() で VkImage を自前確保
//      2. Filament が RenderTarget を通じてその VkImage に描画
//      3. Qt SceneGraph は QNativeInterface::QSGVulkanTexture::fromNative() で
//         同じ VkImage を QSGTexture としてラップして表示 (ゼロコピー)
//
//    利点:
//      XWayland 不要 / Wayland ネイティブ / ゼロコピー / Qt 6.11 完全対応
// ─────────────────────────────────────────────────────────────────────────────

namespace AviQtl::Rendering {

// ─── QtVulkanPlatform ────────────────────────────────────────────────────────

QtVulkanPlatform::~QtVulkanPlatform() { terminate(); }

void QtVulkanPlatform::setSharedContext(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t graphicsQueueFamilyIndex, uint32_t graphicsQueueIndex) {
    m_externalInstance = instance;
    m_externalPhysicalDevice = physicalDevice;
    m_externalDevice = device;
    m_graphicsQueueFamilyIndex = graphicsQueueFamilyIndex;
    m_graphicsQueueIndex = graphicsQueueIndex;
    m_contextReady = true;
}

// Filament が VkInstance を生成しようとした時に呼ばれる。
// Qt のものを横取りして返す。
VkInstance QtVulkanPlatform::createVkInstance(VkInstanceCreateInfo const &) noexcept { return m_externalInstance; }

// Filament が VkPhysicalDevice を選択しようとした時に呼ばれる。
VkPhysicalDevice QtVulkanPlatform::selectVkPhysicalDevice(VkInstance) noexcept { return m_externalPhysicalDevice; }

// Filament が VkDevice を生成しようとした時に呼ばれる。
// Qt のものを横取りして返す。
VkDevice QtVulkanPlatform::createVkDevice(VkDeviceCreateInfo const &) noexcept { return m_externalDevice; }

// ヘッドレス動作: surface は不要
QtVulkanPlatform::SurfaceBundle QtVulkanPlatform::createVkSurfaceKHR(void *, VkInstance, uint64_t) const noexcept { return {VK_NULL_HANDLE, {0, 0}}; }

// カスタム SwapChain を生成する (nativeWindow == nullptr でヘッドレス)
filament::backend::Platform::SwapChain *QtVulkanPlatform::createSwapChain(void *nativeWindow, uint64_t, VkExtent2D extent) {
    if (nativeWindow != nullptr || extent.width == 0 || extent.height == 0) {
        return nullptr;
    }

    auto *sc = allocateImage(extent);
    if (!sc) {
        qCritical() << "[QtVulkanPlatform] VkImage 確保に失敗しました。";
        return nullptr;
    }

    m_currentImage = sc->colorImage;
    m_currentExtent = extent;

    qDebug() << "[QtVulkanPlatform] SwapChain (ヘッドレス) 生成完了:" << extent.width << "x" << extent.height;
    return sc;
}

filament::backend::VulkanPlatform::SwapChainBundle QtVulkanPlatform::getSwapChainBundle(SwapChainPtr handle) {
    const auto *sc = static_cast<QtVulkanSwapChain *>(handle);
    SwapChainBundle bundle;
    bundle.colors = utils::FixedCapacityVector<VkImage>(1);
    bundle.colors[0] = sc->colorImage;
    bundle.colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    bundle.depthFormat = VK_FORMAT_UNDEFINED;
    bundle.extent = sc->extent;
    bundle.layerCount = 1;
    return bundle;
}

VkResult QtVulkanPlatform::acquire(SwapChainPtr, ImageSyncData *outImageSyncData) {
    if (outImageSyncData) {
        outImageSyncData->imageIndex = 0;
        outImageSyncData->imageReadySemaphore = VK_NULL_HANDLE;
    }
    return VK_SUCCESS;
}

VkResult QtVulkanPlatform::present(SwapChainPtr, uint32_t, VkSemaphore) {
    // Qt SceneGraph 側が表示を担うため、ここでは何もしない
    return VK_SUCCESS;
}

bool QtVulkanPlatform::hasResized(SwapChainPtr) { return false; }

VkResult QtVulkanPlatform::recreate(SwapChainPtr) { return VK_SUCCESS; }

void QtVulkanPlatform::destroy(SwapChainPtr handle) {
    deallocateImage(static_cast<QtVulkanSwapChain *>(handle));
    m_currentImage = VK_NULL_HANDLE;
}

void QtVulkanPlatform::terminate() {
    // Qt が所有するデバイスは Qt 側で解放するため、ここでは解放しない
    m_externalInstance = VK_NULL_HANDLE;
    m_externalPhysicalDevice = VK_NULL_HANDLE;
    m_externalDevice = VK_NULL_HANDLE;
    m_contextReady = false;
}

// VkImage をデバイスメモリごと確保する
QtVulkanSwapChain *QtVulkanPlatform::allocateImage(VkExtent2D extent) {
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {extent.width, extent.height, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(m_externalDevice, &imgInfo, nullptr, &image) != VK_SUCCESS) {
        return nullptr;
    }

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(m_externalDevice, image, &memReq);

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(m_externalPhysicalDevice, &memProps);

    uint32_t memTypeIdx = UINT32_MAX;
    const uint32_t desiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReq.memoryTypeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & desiredFlags) == desiredFlags) {
            memTypeIdx = i;
            break;
        }
    }
    if (memTypeIdx == UINT32_MAX) {
        vkDestroyImage(m_externalDevice, image, nullptr);
        return nullptr;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memTypeIdx;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(m_externalDevice, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyImage(m_externalDevice, image, nullptr);
        return nullptr;
    }

    vkBindImageMemory(m_externalDevice, image, memory, 0);

    auto *sc = new QtVulkanSwapChain();
    sc->colorImage = image;
    sc->colorMemory = memory;
    sc->extent = extent;
    return sc;
}

void QtVulkanPlatform::deallocateImage(QtVulkanSwapChain *sc) {
    if (!sc)
        return;
    if (m_externalDevice == VK_NULL_HANDLE)
        return;

    if (sc->colorImage != VK_NULL_HANDLE)
        vkDestroyImage(m_externalDevice, sc->colorImage, nullptr);
    if (sc->colorMemory != VK_NULL_HANDLE)
        vkFreeMemory(m_externalDevice, sc->colorMemory, nullptr);

    delete sc;
}

// ─── FilamentCanvas — ctor / dtor ────────────────────────────────────────────

FilamentCanvas::FilamentCanvas(QQuickItem *parent) : QQuickItem(parent), m_platform(std::make_unique<QtVulkanPlatform>()) {
    setFlag(ItemHasContents, true);
    connect(this, &QQuickItem::windowChanged, this, &FilamentCanvas::handleWindowChanged);
}

FilamentCanvas::~FilamentCanvas() {
    if (m_window) {
        disconnect(m_beforeRenderingConn);
        disconnect(m_sceneGraphInvalidatedConn);
    }
    // QSGTexture は Qt が所有しないため自前で解放
    // ただし fromNative() で生成した場合は nullptr チェックのみ
    delete m_sgTexture;
    m_sgTexture = nullptr;
}

// ─── プロパティ ───────────────────────────────────────────────────────────────

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
    m_frameDirty.store(true, std::memory_order_release);
    update();
}

// ─── ウィンドウ接続 ───────────────────────────────────────────────────────────

void FilamentCanvas::handleWindowChanged(QQuickWindow *win) {
    if (m_window) {
        disconnect(m_beforeRenderingConn);
        disconnect(m_sceneGraphInvalidatedConn);
    }
    m_window = win;
    if (!win)
        return;

    m_beforeRenderingConn = connect(win, &QQuickWindow::beforeRendering, this, &FilamentCanvas::onBeforeRendering, Qt::DirectConnection);
    m_sceneGraphInvalidatedConn = connect(win, &QQuickWindow::sceneGraphInvalidated, this, &FilamentCanvas::onSceneGraphInvalidated, Qt::DirectConnection);
    win->setColor(Qt::transparent);
}

// ─── Filament 初期化 (レンダースレッド) ──────────────────────────────────────

void FilamentCanvas::initFilament() {
    if (m_engine)
        return;

    if (!m_window) {
        qCritical() << "[FilamentCanvas] initFilament: ウィンドウが未接続です。";
        return;
    }

    // Qt RHI から Vulkan コンテキストを取得する
    QSGRendererInterface *rif = m_window->rendererInterface();
    if (!rif) {
        qCritical() << "[FilamentCanvas] QSGRendererInterface が取得できません。";
        return;
    }

    // Qt 6.x: QRhi インスタンス経由で Vulkan ネイティブハンドルを取得
    auto *rhi = static_cast<QRhi *>(rif->getResource(m_window, QSGRendererInterface::RhiResource));
    if (!rhi) {
        qCritical() << "[FilamentCanvas] QRhi が取得できません。"
                       " GraphicsApi = Vulkan を確認してください。";
        return;
    }

    const auto *nativeHandles = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
    if (!nativeHandles) {
        qCritical() << "[FilamentCanvas] QRhiVulkanNativeHandles が取得できません。";
        return;
    }

    m_platform->setSharedContext(nativeHandles->inst->vkInstance(), nativeHandles->physDev, nativeHandles->dev, nativeHandles->gfxQueueFamilyIdx, static_cast<uint32_t>(nativeHandles->gfxQueueIdx));

    qDebug() << "[FilamentCanvas] VulkanSharedContext 設定完了。Filament Engine を初期化します。";

    // VulkanSharedContext を void* にキャストして sharedContext に渡す
    // FilamentのVulkanバックエンドはこれをVulkanSharedContext*にキャストして使う
    filament::backend::VulkanPlatform::VulkanSharedContext sharedCtx{};
    sharedCtx.instance = nativeHandles->inst->vkInstance();
    sharedCtx.physicalDevice = nativeHandles->physDev;
    sharedCtx.logicalDevice = nativeHandles->dev;
    sharedCtx.graphicsQueueFamilyIndex = static_cast<uint32_t>(nativeHandles->gfxQueueFamilyIdx);
    sharedCtx.graphicsQueueIndex = static_cast<uint32_t>(nativeHandles->gfxQueueIdx);

    m_engine = filament::Engine::Builder().backend(filament::Engine::Backend::VULKAN).platform(m_platform.get()).sharedContext(&sharedCtx).build();

    if (!m_engine) {
        qCritical() << "[FilamentCanvas] Engine::Builder::build() 失敗。";
        return;
    }

    m_renderer = m_engine->createRenderer();
    m_scene = m_engine->createScene();
    m_view = m_engine->createView();

    m_cameraEntity = utils::EntityManager::get().create();
    m_camera = m_engine->createCamera(m_cameraEntity);

    m_view->setScene(m_scene);
    m_view->setCamera(m_camera);
    m_view->setPostProcessingEnabled(false);

    // 仕様書準拠: 紺色 (#001A33)
    m_skybox = filament::Skybox::Builder().color({0.0f, 0.1f, 0.2f, 1.0f}).build(*m_engine);
    m_scene->setSkybox(m_skybox);

    qDebug() << "[FilamentCanvas] Filament Engine 初期化完了 (VulkanSharedContextQt)。";
}

// ─── オフスクリーン RenderTarget ─────────────────────────────────────────────

bool FilamentCanvas::recreateOffscreenTarget(uint32_t w, uint32_t h) {
    if (!m_engine || w == 0 || h == 0)
        return false;

    if (m_renderTarget && m_targetW == w && m_targetH == h)
        return true;

    destroyOffscreenTarget();

    qDebug() << "[FilamentCanvas] RenderTarget 生成:" << w << "x" << h;

    // カスタム SwapChain: Qt の VkImage を Filament に渡す
    // nativeWindow = nullptr でヘッドレス、extent 指定
    m_swapChain = m_engine->createSwapChain(nullptr, 0, {w, h});
    if (!m_swapChain) {
        qCritical() << "[FilamentCanvas] createSwapChain(headless) 失敗。";
        return false;
    }

    // RenderTarget 用テクスチャ
    m_colorTexture = filament::Texture::Builder().width(w).height(h).levels(1).usage(filament::Texture::Usage::COLOR_ATTACHMENT | filament::Texture::Usage::SAMPLEABLE).format(filament::Texture::InternalFormat::RGBA8).build(*m_engine);

    m_depthTexture = filament::Texture::Builder().width(w).height(h).levels(1).usage(filament::Texture::Usage::DEPTH_ATTACHMENT).format(filament::Texture::InternalFormat::DEPTH32F).build(*m_engine);

    if (!m_colorTexture || !m_depthTexture) {
        qCritical() << "[FilamentCanvas] Texture 生成失敗。";
        destroyOffscreenTarget();
        return false;
    }

    m_renderTarget = filament::RenderTarget::Builder().texture(filament::RenderTarget::AttachmentPoint::COLOR0, m_colorTexture).texture(filament::RenderTarget::AttachmentPoint::DEPTH, m_depthTexture).build(*m_engine);

    if (!m_renderTarget) {
        qCritical() << "[FilamentCanvas] RenderTarget 生成失敗。";
        destroyOffscreenTarget();
        return false;
    }

    m_view->setRenderTarget(m_renderTarget);
    m_targetW = w;
    m_targetH = h;

    updateViewport(w, h);

    // サイズ変更で古い QSGTexture ラッパーを破棄
    delete m_sgTexture;
    m_sgTexture = nullptr;
    m_lastBoundImage = VK_NULL_HANDLE;

    qDebug() << "[FilamentCanvas] RenderTarget 準備完了。";
    return true;
}

void FilamentCanvas::destroyOffscreenTarget() {
    if (!m_engine)
        return;

    m_view->setRenderTarget(nullptr);

    if (m_renderTarget) {
        m_engine->destroy(m_renderTarget);
        m_renderTarget = nullptr;
    }
    if (m_colorTexture) {
        m_engine->destroy(m_colorTexture);
        m_colorTexture = nullptr;
    }
    if (m_depthTexture) {
        m_engine->destroy(m_depthTexture);
        m_depthTexture = nullptr;
    }
    if (m_swapChain) {
        m_engine->destroy(m_swapChain);
        m_swapChain = nullptr;
    }

    m_targetW = 0;
    m_targetH = 0;
}

// ─── Filament 破棄 (レンダースレッド) ────────────────────────────────────────

void FilamentCanvas::destroyFilament() {
    if (!m_engine)
        return;

    destroyOffscreenTarget();

    if (m_camera) {
        m_engine->destroyCameraComponent(m_cameraEntity);
        utils::EntityManager::get().destroy(m_cameraEntity);
        m_camera = nullptr;
    }
    if (m_skybox) {
        m_engine->destroy(m_skybox);
        m_skybox = nullptr;
    }
    if (m_view) {
        m_engine->destroy(m_view);
        m_view = nullptr;
    }
    if (m_scene) {
        m_engine->destroy(m_scene);
        m_scene = nullptr;
    }
    if (m_renderer) {
        m_engine->destroy(m_renderer);
        m_renderer = nullptr;
    }

    filament::Engine::destroy(&m_engine);
    m_engine = nullptr;

    delete m_sgTexture;
    m_sgTexture = nullptr;
    m_lastBoundImage = VK_NULL_HANDLE;

    qDebug() << "[FilamentCanvas] Filament Engine 破棄完了。";
}

// ─── レンダースレッドコールバック ─────────────────────────────────────────────

void FilamentCanvas::onBeforeRendering() {
    if (!m_window)
        return;

    const double dpr = m_window->devicePixelRatio();
    const uint32_t pw = static_cast<uint32_t>(width() * dpr);
    const uint32_t ph = static_cast<uint32_t>(height() * dpr);

    if (pw == 0 || ph == 0)
        return;

    if (!m_engine) {
        initFilament();
        if (!m_engine)
            return;
    }

    if (!recreateOffscreenTarget(pw, ph))
        return;

    if (m_renderer->beginFrame(m_swapChain)) {
        m_renderer->render(m_view);
        m_renderer->endFrame();
    }

    m_frameDirty.store(true, std::memory_order_release);
    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
}

void FilamentCanvas::onSceneGraphInvalidated() { destroyFilament(); }

// ─── Qt SceneGraph ノード ─────────────────────────────────────────────────────

QSGNode *FilamentCanvas::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    if (!m_platform || !m_window) {
        delete oldNode;
        return nullptr;
    }

    // Filament 描画完了後の VkImage を取得
    const VkImage colorImage = m_platform->currentColorImage();
    if (colorImage == VK_NULL_HANDLE) {
        // まだ描画されていない
        return oldNode;
    }

    if (!m_frameDirty.load(std::memory_order_acquire)) {
        return oldNode;
    }
    m_frameDirty.store(false, std::memory_order_release);

    // VkImage が変わった場合 (サイズ変更) は QSGTexture を再生成
    if (m_lastBoundImage != colorImage) {
        delete m_sgTexture;
        m_sgTexture = nullptr;
        m_lastBoundImage = colorImage;
    }

    // Qt 6.8+ 正式 API: QNativeInterface::QSGVulkanTexture::fromNative()
    // VkImage を QSGTexture としてラップする (ゼロコピー)
    if (!m_sgTexture) {
        auto *vulkanTextureIface = m_window->rendererInterface() ? dynamic_cast<QNativeInterface::QSGVulkanTexture *>(nullptr) : nullptr;
        // 直接 QNativeInterface::QSGVulkanTexture::fromNative を呼ぶ
        m_sgTexture = QNativeInterface::QSGVulkanTexture::fromNative(colorImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_window, QSize(static_cast<int>(m_targetW), static_cast<int>(m_targetH)));
        (void)vulkanTextureIface; // 参照回避

        if (!m_sgTexture) {
            qWarning() << "[FilamentCanvas] QSGVulkanTexture::fromNative() 失敗。";
            delete oldNode;
            return nullptr;
        }
    }

    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setFiltering(QSGTexture::Linear);
    }

    node->setTexture(m_sgTexture);
    node->setRect(boundingRect());
    // Filament は Y 下向き、Qt SceneGraph は Y 上向き → 垂直反転で補正
    node->setTextureCoordinatesTransform(QSGSimpleTextureNode::MirrorVertically);

    return node;
}

// ─── ジオメトリ変更 ───────────────────────────────────────────────────────────

void FilamentCanvas::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        m_targetW = 0;
        m_targetH = 0;
        delete m_sgTexture;
        m_sgTexture = nullptr;
        m_lastBoundImage = VK_NULL_HANDLE;
        update();
    }
}

void FilamentCanvas::itemChange(ItemChange change, const ItemChangeData &value) { QQuickItem::itemChange(change, value); }

// ─── カメラ / ビューポート ────────────────────────────────────────────────────

void FilamentCanvas::updateViewport(uint32_t physW, uint32_t physH) {
    if (!m_view || !m_camera || physW == 0 || physH == 0)
        return;

    m_view->setViewport({0, 0, physW, physH});

    const double dpr = m_window ? m_window->devicePixelRatio() : 1.0;
    const double logW = physW / dpr;
    const double logH = physH / dpr;
    m_camera->setProjection(filament::Camera::Projection::ORTHO, 0.0, logW, logH, 0.0, -1.0, 1.0);
}

} // namespace AviQtl::Rendering
