#pragma once

#include <QMetaObject>
#include <QQuickItem>
#include <QSGSimpleTextureNode>
#include <utils/Entity.h>

#include <atomic>
#include <cstdint>
#include <memory>

// Vulkan ヘッダはFilamentのblueVK経由で取得する
// (VulkanPlatform.hがbluevk/BlueVK.hをインクルード済みのため二重定義を避ける)
#include <backend/platforms/VulkanPlatform.h>

namespace filament {
class Engine;
class Renderer;
class Scene;
class View;
class Camera;
class SwapChain;
class Skybox;
class RenderTarget;
class Texture;
} // namespace filament

namespace AviQtl::Rendering {

// ─────────────────────────────────────────────────────────────────────────────
// QtVulkanPlatform  (VulkanPlatform のカスタム実装)
//
// Qt の QRhi (Vulkan バックエンド) から取得した VkInstance / VkDevice を
// Filament に渡し、同一デバイス上でゼロコピー描画を実現する。
//
// カスタム SwapChain:
//   nativeWindow = nullptr の時は自前で vkCreateImage を発行し、
//   その VkImage を SwapChainBundle に詰めて返す。
//   Qt SceneGraph 側は QNativeInterface::QSGVulkanTexture::fromNative() で
//   同じ VkImage を QSGTexture としてラップして使う。
// ─────────────────────────────────────────────────────────────────────────────
class QtVulkanSwapChain : public filament::backend::Platform::SwapChain {
  public:
    VkImage colorImage = VK_NULL_HANDLE;
    VkDeviceMemory colorMemory = VK_NULL_HANDLE;
    VkExtent2D extent = {0, 0};
};

class QtVulkanPlatform final : public filament::backend::VulkanPlatform {
  public:
    explicit QtVulkanPlatform() = default;
    ~QtVulkanPlatform() override;

    // Qt RHI から取得した Vulkan コンテキストを設定する (initFilament() 前に呼ぶ)
    void setSharedContext(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t graphicsQueueFamilyIndex, uint32_t graphicsQueueIndex);

    bool isContextReady() const noexcept { return m_contextReady; }

    // VulkanPlatform 純粋仮想関数の実装
    // ヘッドレス動作のため surface は不要
    ExtensionSet getSwapchainInstanceExtensions() const override { return {}; }

    // カスタム SwapChain 管理
    SwapChainPtr createSwapChain(void *nativeWindow, uint64_t flags, VkExtent2D extent) override;
    SwapChainBundle getSwapChainBundle(SwapChainPtr handle) override;
    VkResult acquire(SwapChainPtr handle, ImageSyncData *outImageSyncData) override;
    VkResult present(SwapChainPtr handle, uint32_t index, VkSemaphore finishedDrawing) override;
    bool hasResized(SwapChainPtr handle) override;
    VkResult recreate(SwapChainPtr handle) override;
    void destroy(SwapChainPtr handle) override;
    void terminate() override;

    // Qt SceneGraph への橋渡し: 現在の描画済み VkImage を返す
    VkImage currentColorImage() const noexcept { return m_currentImage; }
    VkExtent2D currentExtent() const noexcept { return m_currentExtent; }

  protected:
    // sharedContext を受け取ったら VkInstance / VkDevice 作成をスキップする
    VkInstance createVkInstance(VkInstanceCreateInfo const &createInfo) noexcept override;
    VkPhysicalDevice selectVkPhysicalDevice(VkInstance instance) noexcept override;
    VkDevice createVkDevice(VkDeviceCreateInfo const &createInfo) noexcept override;

    SurfaceBundle createVkSurfaceKHR(void *nativeWindow, VkInstance instance, uint64_t flags) const noexcept override;

  private:
    QtVulkanSwapChain *allocateImage(VkExtent2D extent);
    void deallocateImage(QtVulkanSwapChain *sc);

    bool m_contextReady = false;

    VkInstance m_externalInstance = VK_NULL_HANDLE;
    VkPhysicalDevice m_externalPhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_externalDevice = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamilyIndex = 0;
    uint32_t m_graphicsQueueIndex = 0;

    VkImage m_currentImage = VK_NULL_HANDLE;
    VkExtent2D m_currentExtent = {0, 0};
};

// ─────────────────────────────────────────────────────────────────────────────
// FilamentCanvas  (フェーズ2 — VulkanSharedContextQt + QSGVulkanTexture)
//
// アーキテクチャ:
//   QtVulkanPlatform が Qt の RHI (Vulkan) と Filament を同一 VkDevice で接続する。
//   Filament は自前の VkImage に描画し、Qt SceneGraph はその VkImage を
//   QNativeInterface::QSGVulkanTexture::fromNative() でラップして表示する。
//   XWayland 不要・Wayland ネイティブ・ゼロコピー動作。
//
//   ┌─────────────────┐  同一VkDevice  ┌──────────────────┐
//   │  Qt RHI(Vulkan) │ ◀──────────▶ │ Filament (Vulkan) │
//   │  QQuickWindow   │               │  QtVulkanPlatform │
//   └────────┬────────┘               └────────┬──────────┘
//            │ QSGVulkanTexture::fromNative()   │ VkImage (color attachment)
//            └─────────────────────────────────┘
//
// Phase: フェーズ2 (VulkanSharedContextQt 実装)
// ─────────────────────────────────────────────────────────────────────────────
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
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

  private slots:
    void handleWindowChanged(QQuickWindow *win);
    void onBeforeRendering();
    void onSceneGraphInvalidated();

  private:
    void initFilament();
    void destroyFilament();

    bool recreateOffscreenTarget(uint32_t w, uint32_t h);
    void destroyOffscreenTarget();
    void updateViewport(uint32_t physW, uint32_t physH);

    int m_sceneId = -1;
    int m_currentFrame = 0;

    QQuickWindow *m_window = nullptr;
    QMetaObject::Connection m_beforeRenderingConn;
    QMetaObject::Connection m_sceneGraphInvalidatedConn;

    // カスタム Vulkan プラットフォーム (Filament ←→ Qt RHI の接点)
    std::unique_ptr<QtVulkanPlatform> m_platform;

    // Filament コアオブジェクト
    filament::Engine *m_engine = nullptr;
    filament::Renderer *m_renderer = nullptr;
    filament::Scene *m_scene = nullptr;
    filament::View *m_view = nullptr;
    filament::Camera *m_camera = nullptr;
    filament::Skybox *m_skybox = nullptr;

    // カスタム SwapChain (QtVulkanPlatform が所有する VkImage を指す)
    filament::SwapChain *m_swapChain = nullptr;

    // オフスクリーン RenderTarget (カラー + 深度)
    filament::RenderTarget *m_renderTarget = nullptr;
    filament::Texture *m_colorTexture = nullptr;
    filament::Texture *m_depthTexture = nullptr;

    utils::Entity m_cameraEntity;

    uint32_t m_targetW = 0;
    uint32_t m_targetH = 0;

    std::atomic<bool> m_frameDirty{false};

    // QSGTexture ラッパー (fromNative() で生成、毎フレーム再生成しない)
    QSGTexture *m_sgTexture = nullptr;
    // 直前の VkImage (サイズ変更検出用)
    VkImage m_lastBoundImage = VK_NULL_HANDLE;
};

} // namespace AviQtl::Rendering
