#include "preview_render_thread.hpp"

#include <QDebug>
#include <cmath>

// bgfx
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#if defined(__APPLE__)
#include <SDL3/SDL_metal.h>
#endif

namespace AviQtl {
namespace Render {

namespace {
uint32_t getRainbowColor(float hue) {
  float h = hue * 6.0f;
  int i = static_cast<int>(std::floor(h));
  float f = h - static_cast<float>(i);
  float q = 1.0f - f;
  float r = 0, g = 0, b = 0;
  switch (i % 6) {
  case 0:
    r = 1.f;
    g = f;
    b = 0;
    break;
  case 1:
    r = q;
    g = 1.f;
    b = 0;
    break;
  case 2:
    r = 0;
    g = 1.f;
    b = f;
    break;
  case 3:
    r = 0;
    g = q;
    b = 1.f;
    break;
  case 4:
    r = f;
    g = 0;
    b = 1.f;
    break;
  case 5:
    r = 1.f;
    g = 0;
    b = q;
    break;
  }
  return (static_cast<uint8_t>(r * 255) << 24) |
         (static_cast<uint8_t>(g * 255) << 16) |
         (static_cast<uint8_t>(b * 255) << 8) | 0xFF;
}
} // namespace

PreviewRenderThread::PreviewRenderThread(QObject *parent) : QThread(parent) {
  qDebug() << "[PreviewRenderThread] constructed";
}

PreviewRenderThread::~PreviewRenderThread() {
  m_running = false;
  m_condition.wakeAll();
  wait();
  qDebug() << "[PreviewRenderThread] worker thread joined";
}

void PreviewRenderThread::setNativeWindowInfo(void *nwh, void *ndt, int width,
                                              int height,
                                              WindowSubsystem subsystem) {
  m_nwh = nwh;
  m_ndt = ndt;
  m_width = width;
  m_height = height;
  m_subsystem = subsystem;
  qDebug() << "[PreviewRenderThread] setNativeWindowInfo:"
           << "nwh=" << nwh << "ndt=" << ndt << "size=" << width << "x"
           << height << "subsystem=" << static_cast<int>(subsystem);
}

void PreviewRenderThread::requestFrameRender(int64_t frameNumber) {
  QMutexLocker lock(&m_mutex);
  m_requestedFrame = frameNumber;
  m_condition.wakeAll();
}

void PreviewRenderThread::requestResize(int width, int height) {
  QMutexLocker lock(&m_mutex);
  m_pendingWidth = width;
  m_pendingHeight = height;
  m_pendingResize = true;
  m_condition.wakeAll();
}

void PreviewRenderThread::initBgfx() {
  qDebug() << "[PreviewRenderThread::initBgfx] thread="
           << QThread::currentThreadId();

  if (!m_nwh) {
    qCritical() << "[PreviewRenderThread::initBgfx] FATAL: nwh is null!";
    m_bgfxReady = false;
    return;
  }

  bgfx::PlatformData pd{};
  pd.nwh = m_nwh;

#if defined(__linux__)
  if (m_subsystem == WindowSubsystem::Wayland) {
    pd.ndt = m_ndt;
    pd.type = bgfx::NativeWindowHandleType::Wayland;
    qDebug()
        << "[PreviewRenderThread::initBgfx] NativeWindowHandleType::Wayland";
  } else {
    pd.ndt = m_ndt;
    pd.type = bgfx::NativeWindowHandleType::Default; // Xlib
    qDebug() << "[PreviewRenderThread::initBgfx] "
                "NativeWindowHandleType::Default (X11)";
  }
#elif defined(__APPLE__)
  pd.ndt = nullptr;
  pd.type = bgfx::NativeWindowHandleType::Default;
#else
  pd.ndt = nullptr;
  pd.type = bgfx::NativeWindowHandleType::Default;
#endif

  // bgfx シングルスレッドモードを宣言する。
  // bgfx::renderFrame(-1) を init() より先に呼ぶと、bgfx はレンダースレッドを
  // 内部生成せず、呼び出し元スレッドをレンダースレッドとして使う。
  // これにより g_platformData.type が SwapChainVK::createSurface() の
  // Wayland/X11 分岐に確実に反映される。
  bgfx::renderFrame(-1);

  bgfx::Init init{};
  init.platformData = pd;

#if defined(__APPLE__)
  init.type = bgfx::RendererType::Metal;
#else
  init.type = bgfx::RendererType::Vulkan;
#endif
  init.resolution.width = static_cast<uint32_t>(m_width);
  init.resolution.height = static_cast<uint32_t>(m_height);
  init.resolution.reset = BGFX_RESET_VSYNC;

  qDebug() << "[PreviewRenderThread::initBgfx] calling bgfx::init()"
           << bgfx::getRendererName(init.type) << m_width << "x" << m_height;

  if (!bgfx::init(init)) {
    qCritical()
        << "[PreviewRenderThread::initBgfx] FATAL: bgfx::init() failed!";
    m_bgfxReady = false;
    return;
  }

  qDebug() << "[PreviewRenderThread::initBgfx] success:"
           << bgfx::getRendererName(bgfx::getRendererType());
  m_bgfxReady = true;
}

void PreviewRenderThread::shutdownBgfx() {
  if (m_bgfxReady) {
    bgfx::shutdown();
    m_bgfxReady = false;
    qDebug() << "[PreviewRenderThread::shutdownBgfx] done";
  }
}

void PreviewRenderThread::run() {
  qDebug() << "[PreviewRenderThread::run] started, id="
           << QThread::currentThreadId();

  initBgfx();
  if (!m_bgfxReady) {
    qCritical() << "[PreviewRenderThread::run] bgfx init failed; exiting.";
    return;
  }

  while (m_running) {
    int64_t frameToRender = -1;
    bool doResize = false;
    int newW = 0, newH = 0;

    {
      QMutexLocker lock(&m_mutex);
      if (m_running && m_requestedFrame == -1 && !m_pendingResize)
        m_condition.wait(&m_mutex, 16);

      if (!m_running)
        break;

      if (m_pendingResize) {
        doResize = true;
        newW = m_pendingWidth;
        newH = m_pendingHeight;
        m_pendingResize = false;
      }
      if (m_requestedFrame != -1) {
        frameToRender = m_requestedFrame;
        m_requestedFrame = -1;
      } else {
        frameToRender = m_frameCounter;
      }
    }

    if (doResize) {
      bgfx::reset(static_cast<uint32_t>(newW), static_cast<uint32_t>(newH),
                  BGFX_RESET_VSYNC);
      m_width = newW;
      m_height = newH;
      qDebug() << "[PreviewRenderThread::run] bgfx::reset" << newW << "x"
               << newH;
      frameToRender = m_frameCounter;
    }

    if (frameToRender < 0)
      continue;

    float hue = static_cast<float>(m_frameCounter % 300) / 300.0f;
    uint32_t clearColor = getRainbowColor(hue);
    m_frameCounter++;

    bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(m_width),
                      static_cast<uint16_t>(m_height));
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clearColor, 1.0f,
                       0);
    bgfx::touch(0);
    bgfx::frame();
  }

  shutdownBgfx();
  qDebug() << "[PreviewRenderThread::run] exiting";
}

} // namespace Render
} // namespace AviQtl
