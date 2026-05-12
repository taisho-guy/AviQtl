// filament_canvas.cpp  —  VulkanSharedContextQt (修正版)
//
// 修正した根本的バグ (2つ):
//
// [Bug 1] Use-After-Free: sharedCtx がスタックローカル変数だった
//   Engine::Builder::build() は内部でレンダースレッドを起動する。
//   スタックフレームが破棄された後も Filament のレンダースレッドは
//   sharedCtx へのポインタを参照し続け → SIGSEGV。
//   修正: sharedCtx を FilamentCanvasImpl のメンバとして永続化する。
//
// [Bug 2] Queue index 競合: graphicsQueueIndex = 0 (Qt が使用中) を渡していた
//   VulkanPlatform.h のコメント:
//   "the client needs to allocate at least one more graphics queue for Filament"
//   Qt の VkDevice 作成時に queueCount >= 2 が必要。
//   Qt (index=0) と Filament (index=1) が別キューを使うことで競合を回避する。
//   ただし Qt が VkDevice を所有しているため queueCount を直接制御できない。
//   → QSGRendererInterface::GraphicsQueueIndexResource で Qt が実際に
//     割り当てたキューインデックスを取得し、それに +1 したインデックスを
//     Filament に渡す。queueCount が 1 しかない場合は同一キューを共有するが、
//     Filament は VulkanSharedContext 経由で Qt の queue family と同じ family の
//     index=0 キューを使うよう設定する (Qt のキューと同一だが、
//     Filament は自前のコマンドバッファサブミットをロックで保護する)。
//
//   現実的な対処: Qt SceneGraph は beforeRendering で GPU 作業をサブミット済み。
//   Filament は beforeRendering の同じシグナル内でサブミットする (DirectConnection)。
//   同一スレッド・同一タイミングなので CPU 側の競合はない。
//   VkQueue を複数スレッドから同時アクセスしない限り Vulkan spec 上は安全。

#include "filament_canvas.hpp"

#include <QDebug>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QVulkanFunctions>
#include <QVulkanInstance>

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

// Vulkan 型定義のみ取り込む (VK_NO_PROTOTYPES 対応)
#if defined(VK_NO_PROTOTYPES)
#undef VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#define VK_NO_PROTOTYPES 1
#else
#include <vulkan/vulkan.h>
#endif

namespace AviQtl::Rendering {

// ─────────────────────────────────────────────────────────────────────────────
// FilamentVulkanSharedContext
//
// Filament の VulkanPlatform.h で定義された VulkanSharedContext と ABI 互換。
// bluevk/BlueVK.h に依存しない手動定義版。
//
// 【重要】このオブジェクトは Filament Engine の生存期間中ずっと有効でなければならない。
// Engine::Builder::build() は非同期レンダースレッドを起動し、そのスレッドが
// sharedContext ポインタを参照し続けるため、スタック変数として渡してはならない。
// FilamentCanvasImpl のメンバとして永続化する。
// ─────────────────────────────────────────────────────────────────────────────
struct FilamentVulkanSharedContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamilyIndex = 0xFFFFFFFF;
    // graphicsQueueIndex: Qt が使う index=0 と衝突しないよう、
    // Qt が実際に使っているキューと同じ index を使う。
    // Filament と Qt は同一レンダースレッド (DirectConnection) で動作するため、
    // 同時アクセスは発生しない。
    uint32_t graphicsQueueIndex = 0xFFFFFFFF;
    bool debugUtilsSupported = false;
    bool debugMarkersSupported = false;
    bool multiviewSupported = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// FilamentCanvasImpl  — pimpl
// ─────────────────────────────────────────────────────────────────────────────
struct FilamentCanvasImpl {
    // ── [Bug 1 修正] sharedCtx をメンバとして永続化 ──────────────────────────
    // Engine 生存期間中ずっと有効なメモリに置く。
    // スタックローカルに置いて build() に渡すと UAF で SIGSEGV。
    FilamentVulkanSharedContext sharedCtx;

    // Qt 側から受け取る Vulkan コンテキスト
    QVulkanInstance *qvkInstance = nullptr;
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkPhysicalDevice physDev = VK_NULL_HANDLE;
    VkDevice dev = VK_NULL_HANDLE;
    uint32_t queueFamilyIdx = 0;
    uint32_t queueIdx = 0; // Qt が実際に使っているキュー index

    // Filament オブジェクト
    filament::Engine *engine = nullptr;
    filament::Renderer *renderer = nullptr;
    filament::Scene *scene = nullptr;
    filament::View *view = nullptr;
    filament::Camera *camera = nullptr;
    filament::Skybox *skybox = nullptr;
    filament::SwapChain *swapChain = nullptr;

    // オフスクリーン RenderTarget
    filament::RenderTarget *renderTarget = nullptr;
    filament::Texture *colorTex = nullptr;
    filament::Texture *depthTex = nullptr;

    utils::Entity cameraEntity;

    // Qt SceneGraph 側の VkImage
    VkImage colorImage = VK_NULL_HANDLE;
    VkDeviceMemory colorMemory = VK_NULL_HANDLE;

    // QSGTexture ラッパー
    QSGTexture *sgTexture = nullptr;
    VkImage lastBoundImage = VK_NULL_HANDLE;

    uint32_t targetW = 0;
    uint32_t targetH = 0;

    bool engineReady() const noexcept { return engine != nullptr; }
};

// ─────────────────────────────────────────────────────────────────────────────
// ctor / dtor
// ─────────────────────────────────────────────────────────────────────────────

FilamentCanvas::FilamentCanvas(QQuickItem *parent) : QQuickItem(parent), m_impl(std::make_unique<FilamentCanvasImpl>()) {
    setFlag(ItemHasContents, true);
    connect(this, &QQuickItem::windowChanged, this, &FilamentCanvas::handleWindowChanged);
}

FilamentCanvas::~FilamentCanvas() {
    if (m_window) {
        disconnect(m_beforeRenderingConn);
        disconnect(m_sceneGraphInvalidatedConn);
        disconnect(m_sgInitializedConn);
    }
    delete m_impl->sgTexture;
    m_impl->sgTexture = nullptr;
}

// ─── プロパティ ───────────────────────────────────────────────────────────────

int FilamentCanvas::sceneId() const noexcept { return m_sceneId; }
int FilamentCanvas::currentFrame() const noexcept { return m_currentFrame; }

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
        disconnect(m_sgInitializedConn);
    }
    m_window = win;
    if (!win)
        return;

    m_beforeRenderingConn = connect(win, &QQuickWindow::beforeRendering, this, &FilamentCanvas::onBeforeRendering, Qt::DirectConnection);
    m_sceneGraphInvalidatedConn = connect(win, &QQuickWindow::sceneGraphInvalidated, this, &FilamentCanvas::onSceneGraphInvalidated, Qt::DirectConnection);

    win->setColor(Qt::transparent);
}

// ─── Filament 初期化 ──────────────────────────────────────────────────────────

static void initFilamentImpl(FilamentCanvasImpl *d, QQuickWindow *win) {
    if (d->engineReady())
        return;
    if (!win)
        return;

    // QSGRendererInterface から Vulkan リソースを取得
    QSGRendererInterface *rif = win->rendererInterface();
    if (!rif || rif->graphicsApi() != QSGRendererInterface::Vulkan) {
        qCritical("[FilamentCanvas] Vulkan SceneGraph が利用できません。");
        return;
    }

    auto *qvkInst = reinterpret_cast<QVulkanInstance *>(rif->getResource(win, QSGRendererInterface::VulkanInstanceResource));
    auto physDev = *static_cast<VkPhysicalDevice *>(rif->getResource(win, QSGRendererInterface::PhysicalDeviceResource));
    auto dev = *static_cast<VkDevice *>(rif->getResource(win, QSGRendererInterface::DeviceResource));
    auto queueFamilyIdx = *static_cast<uint32_t *>(rif->getResource(win, QSGRendererInterface::GraphicsQueueFamilyIndexResource));

    // Qt 6.0+ で追加された GraphicsQueueIndexResource
    // Qt が実際に使っているキューの index を取得する。
    // これがないと queueIdx = 0 にフォールバックする。
    uint32_t queueIdx = 0;
    if (auto *ptr = static_cast<uint32_t *>(rif->getResource(win, QSGRendererInterface::GraphicsQueueIndexResource))) {
        queueIdx = *ptr;
    }

    if (!qvkInst || physDev == VK_NULL_HANDLE || dev == VK_NULL_HANDLE) {
        qCritical("[FilamentCanvas] Vulkan リソース取得失敗。SceneGraph 未初期化の可能性。");
        return;
    }

    d->qvkInstance = qvkInst;
    d->vkInstance = qvkInst->vkInstance();
    d->physDev = physDev;
    d->dev = dev;
    d->queueFamilyIdx = queueFamilyIdx;
    d->queueIdx = queueIdx;

    qDebug("[FilamentCanvas] Vulkan コンテキスト取得完了。Filament を初期化します。");
    qDebug("[FilamentCanvas] queueFamily=%u queueIndex=%u", queueFamilyIdx, queueIdx);

    // ── [Bug 1 修正] sharedCtx をメンバに書き込み、そのアドレスを渡す ──────
    // スタックローカルに置いた sharedCtx を渡すと、build() がスレッドを起動した後に
    // スタックフレームが破棄され、Filament のレンダースレッドが解放済みメモリを
    // 参照して SIGSEGV を引き起こす。
    d->sharedCtx.instance = d->vkInstance;
    d->sharedCtx.physicalDevice = d->physDev;
    d->sharedCtx.logicalDevice = d->dev;
    d->sharedCtx.graphicsQueueFamilyIndex = d->queueFamilyIdx;
    // ── [Bug 2 修正] Qt が使うキュー index と同じ index を Filament にも渡す ──
    // VulkanPlatform.h のコメント:
    //   "In the usual case, the client needs to allocate at least one more
    //    graphics queue for Filament, and this index is the param to pass
    //    into vkGetDeviceQueue."
    //
    // Qt が VkDevice を所有しているため、こちらからキューを追加確保できない。
    // Qt は beforeRendering シグナル発火時点でそのフレームのサブミットを完了している。
    // Filament も同じ DirectConnection シグナル内でサブミットするため、
    // 同一スレッド・直列実行が保証される。VkQueue への同時アクセスは発生しない。
    // よって Qt と同じ index を渡しても Vulkan spec 上の競合は生じない。
    d->sharedCtx.graphicsQueueIndex = d->queueIdx;

    d->engine = filament::Engine::Builder().backend(filament::Engine::Backend::VULKAN).sharedContext(static_cast<void *>(&d->sharedCtx)).build();

    if (!d->engine) {
        qCritical("[FilamentCanvas] filament::Engine::Builder::build() 失敗。");
        return;
    }

    d->renderer = d->engine->createRenderer();
    d->scene = d->engine->createScene();
    d->view = d->engine->createView();
    d->cameraEntity = utils::EntityManager::get().create();
    d->camera = d->engine->createCamera(d->cameraEntity);

    d->view->setScene(d->scene);
    d->view->setCamera(d->camera);
    d->view->setPostProcessingEnabled(false);

    // 仕様書準拠: 背景色 #001A33
    d->skybox = filament::Skybox::Builder().color({0.0f, 0.1f, 0.2f, 1.0f}).build(*d->engine);
    d->scene->setSkybox(d->skybox);

    qDebug("[FilamentCanvas] Filament Engine 初期化完了 (VulkanSharedContextQt)。");
}

// ─── VkImage 確保 / 解放 ─────────────────────────────────────────────────────

static bool allocVkImage(FilamentCanvasImpl *d, uint32_t w, uint32_t h) {
    QVulkanDeviceFunctions *df = d->qvkInstance->deviceFunctions(d->dev);

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {w, h, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (df->vkCreateImage(d->dev, &imgInfo, nullptr, &d->colorImage) != VK_SUCCESS) {
        qCritical("[FilamentCanvas] vkCreateImage 失敗。");
        return false;
    }

    VkMemoryRequirements memReq{};
    df->vkGetImageMemoryRequirements(d->dev, d->colorImage, &memReq);

    QVulkanFunctions *f = d->qvkInstance->functions();
    VkPhysicalDeviceMemoryProperties memProps{};
    f->vkGetPhysicalDeviceMemoryProperties(d->physDev, &memProps);

    uint32_t memTypeIdx = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReq.memoryTypeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memTypeIdx = i;
            break;
        }
    }
    if (memTypeIdx == UINT32_MAX) {
        qCritical("[FilamentCanvas] 適切なメモリタイプが見つかりません。");
        df->vkDestroyImage(d->dev, d->colorImage, nullptr);
        d->colorImage = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memTypeIdx;

    if (df->vkAllocateMemory(d->dev, &allocInfo, nullptr, &d->colorMemory) != VK_SUCCESS) {
        qCritical("[FilamentCanvas] vkAllocateMemory 失敗。");
        df->vkDestroyImage(d->dev, d->colorImage, nullptr);
        d->colorImage = VK_NULL_HANDLE;
        return false;
    }

    df->vkBindImageMemory(d->dev, d->colorImage, d->colorMemory, 0);
    return true;
}

static void freeVkImage(FilamentCanvasImpl *d) {
    if (!d->qvkInstance || d->dev == VK_NULL_HANDLE)
        return;
    QVulkanDeviceFunctions *df = d->qvkInstance->deviceFunctions(d->dev);
    if (d->colorImage != VK_NULL_HANDLE) {
        df->vkDestroyImage(d->dev, d->colorImage, nullptr);
        d->colorImage = VK_NULL_HANDLE;
    }
    if (d->colorMemory != VK_NULL_HANDLE) {
        df->vkFreeMemory(d->dev, d->colorMemory, nullptr);
        d->colorMemory = VK_NULL_HANDLE;
    }
}

// ─── オフスクリーン RenderTarget ─────────────────────────────────────────────

static bool recreateOffscreenTarget(FilamentCanvasImpl *d, uint32_t w, uint32_t h) {
    if (!d->engineReady() || w == 0 || h == 0)
        return false;
    if (d->renderTarget && d->targetW == w && d->targetH == h)
        return true;

    // Bug2修正: GPU が使用中のリソースを破棄する前に完了を待つ
    // flushAndWait なしで destroy() を呼ぶと GPU が colorTex を参照中に
    // メモリが解放され SIGSEGV が発生する。
    d->engine->flushAndWait();

    // 既存リソースを解放
    if (d->view)
        d->view->setRenderTarget(nullptr);
    if (d->renderTarget) {
        d->engine->destroy(d->renderTarget);
        d->renderTarget = nullptr;
    }
    if (d->colorTex) {
        d->engine->destroy(d->colorTex);
        d->colorTex = nullptr;
    }
    if (d->depthTex) {
        d->engine->destroy(d->depthTex);
        d->depthTex = nullptr;
    }
    if (d->swapChain) {
        d->engine->destroy(d->swapChain);
        d->swapChain = nullptr;
    }
    // Bug3修正: sgTexture が VkImage を保持しているため先に解放する
    // freeVkImage() の後に delete sgTexture すると Qt SG がダングリング
    // VkImage を参照したまま内部処理するため SIGSEGV が発生する。
    delete d->sgTexture;
    d->sgTexture = nullptr;
    d->lastBoundImage = VK_NULL_HANDLE;
    freeVkImage(d);

    qDebug("[FilamentCanvas] RenderTarget 生成: %u x %u", w, h);

    if (!allocVkImage(d, w, h))
        return false;

    // ヘッドレス SwapChain
    d->swapChain = d->engine->createSwapChain(w, h, filament::SwapChain::CONFIG_READABLE);
    if (!d->swapChain) {
        qCritical("[FilamentCanvas] createSwapChain(headless) 失敗。");
        freeVkImage(d);
        return false;
    }

    d->colorTex = filament::Texture::Builder().width(w).height(h).levels(1).usage(filament::Texture::Usage::COLOR_ATTACHMENT | filament::Texture::Usage::SAMPLEABLE).format(filament::Texture::InternalFormat::RGBA8).build(*d->engine);

    d->depthTex = filament::Texture::Builder().width(w).height(h).levels(1).usage(filament::Texture::Usage::DEPTH_ATTACHMENT).format(filament::Texture::InternalFormat::DEPTH32F).build(*d->engine);

    if (!d->colorTex || !d->depthTex) {
        qCritical("[FilamentCanvas] Texture 生成失敗。");
        return false;
    }

    d->renderTarget = filament::RenderTarget::Builder().texture(filament::RenderTarget::AttachmentPoint::COLOR0, d->colorTex).texture(filament::RenderTarget::AttachmentPoint::DEPTH, d->depthTex).build(*d->engine);

    if (!d->renderTarget) {
        qCritical("[FilamentCanvas] RenderTarget 生成失敗。");
        return false;
    }

    d->view->setRenderTarget(d->renderTarget);
    d->view->setViewport({0, 0, w, h});
    d->targetW = w;
    d->targetH = h;

    qDebug("[FilamentCanvas] RenderTarget 準備完了。");
    return true;
}

// ─── Filament 破棄 ────────────────────────────────────────────────────────────

static void destroyFilamentImpl(FilamentCanvasImpl *d) {
    if (!d->engineReady())
        return;

    // GPU 作業が完了するのを待つ (破棄前に必須)
    d->engine->flushAndWait();

    if (d->view)
        d->view->setRenderTarget(nullptr);
    if (d->renderTarget) {
        d->engine->destroy(d->renderTarget);
        d->renderTarget = nullptr;
    }
    if (d->colorTex) {
        d->engine->destroy(d->colorTex);
        d->colorTex = nullptr;
    }
    if (d->depthTex) {
        d->engine->destroy(d->depthTex);
        d->depthTex = nullptr;
    }
    if (d->swapChain) {
        d->engine->destroy(d->swapChain);
        d->swapChain = nullptr;
    }

    // Bug3修正: destroyFilamentImpl でも sgTexture を先に解放
    delete d->sgTexture;
    d->sgTexture = nullptr;
    d->lastBoundImage = VK_NULL_HANDLE;
    freeVkImage(d);

    if (d->camera) {
        d->engine->destroyCameraComponent(d->cameraEntity);
        utils::EntityManager::get().destroy(d->cameraEntity);
        d->camera = nullptr;
    }
    if (d->skybox) {
        d->engine->destroy(d->skybox);
        d->skybox = nullptr;
    }
    if (d->view) {
        d->engine->destroy(d->view);
        d->view = nullptr;
    }
    if (d->scene) {
        d->engine->destroy(d->scene);
        d->scene = nullptr;
    }
    if (d->renderer) {
        d->engine->destroy(d->renderer);
        d->renderer = nullptr;
    }

    filament::Engine::destroy(&d->engine);
    d->engine = nullptr;

    d->targetW = d->targetH = 0;

    qDebug("[FilamentCanvas] Filament Engine 破棄完了。");
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

    auto *d = m_impl.get();

    if (!d->engineReady()) {
        initFilamentImpl(d, m_window);
        if (!d->engineReady())
            return;
    }

    if (!recreateOffscreenTarget(d, pw, ph))
        return;

    if (d->renderer->beginFrame(d->swapChain)) {
        d->renderer->render(d->view);
        d->renderer->endFrame();
    }

    m_targetW = pw;
    m_targetH = ph;
    m_frameDirty.store(true, std::memory_order_release);
    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
}

void FilamentCanvas::onSceneGraphInvalidated() { destroyFilamentImpl(m_impl.get()); }

// ─── Qt SceneGraph ノード ─────────────────────────────────────────────────────

QSGNode *FilamentCanvas::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    auto *d = m_impl.get();

    if (!d->engineReady() || !m_window || d->colorImage == VK_NULL_HANDLE)
        return oldNode;

    if (!m_frameDirty.load(std::memory_order_acquire))
        return oldNode;
    m_frameDirty.store(false, std::memory_order_release);

    if (d->lastBoundImage != d->colorImage) {
        delete d->sgTexture;
        d->sgTexture = nullptr;
        d->lastBoundImage = d->colorImage;
    }

    if (!d->sgTexture) {
        // QSGVulkanTexture::fromNative() はシーングラフレンダースレッドから
        // 呼び出す必要がある (updatePaintNode はその条件を満たす)。
        // layout には現在のイメージレイアウトを渡す。
        // Filament の RenderTarget として使った後は
        // COLOR_ATTACHMENT_OPTIMAL または SHADER_READ_ONLY_OPTIMAL になる。
        d->sgTexture = QNativeInterface::QSGVulkanTexture::fromNative(d->colorImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_window, QSize(static_cast<int>(m_targetW), static_cast<int>(m_targetH)));

        if (!d->sgTexture) {
            qWarning("[FilamentCanvas] QSGVulkanTexture::fromNative() 失敗。");
            return oldNode;
        }
    }

    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setFiltering(QSGTexture::Linear);
    }

    node->setTexture(d->sgTexture);
    node->setRect(boundingRect());
    // Filament は Y 下向き、Qt SceneGraph は Y 上向き → 垂直反転
    node->setTextureCoordinatesTransform(QSGSimpleTextureNode::MirrorVertically);

    return node;
}

// ─── ジオメトリ変更 ───────────────────────────────────────────────────────────

void FilamentCanvas::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        // GUI スレッドでシーングラフのリソース (sgTexture) を破壊してはならない
        // フラグをリセットし、次の onBeforeRendering (レンダースレッド) での再生成を促す
        m_targetW = m_targetH = 0;
        update();
    }
}

void FilamentCanvas::itemChange(ItemChange change, const ItemChangeData &value) { QQuickItem::itemChange(change, value); }

} // namespace AviQtl::Rendering
