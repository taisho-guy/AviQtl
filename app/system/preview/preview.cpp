#include "preview.hpp"

#include <QDebug>
#include <cmath>

// SDL3
#include <SDL3/SDL.h>

// bgfx
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

// macOS Metal ブリッジ
#if defined(SDL_PLATFORM_MACOS)
#include <SDL3/SDL_metal.h>
#endif

namespace AviQtl {
namespace System {

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  内部ヘルパー — 虹色クリアカラー (動作確認用)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
static uint32_t getRainbowColor(float hue) {
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

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  VideoRenderWorker
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
VideoRenderWorker::VideoRenderWorker(QObject *parent) : QThread(parent) {
  qDebug() << "[VideoRenderWorker] constructed";
}

VideoRenderWorker::~VideoRenderWorker() {
  m_running = false;
  m_condition.wakeAll();
  wait();
  qDebug() << "[VideoRenderWorker] worker thread joined";
}

void VideoRenderWorker::setNativeWindowInfo(void *nwh, void *ndt, int width,
                                            int height,
                                            WindowSubsystem subsystem) {
  m_nwh = nwh;
  m_ndt = ndt;
  m_width = width;
  m_height = height;
  m_subsystem = subsystem;
  qDebug() << "[VideoRenderWorker] setNativeWindowInfo:"
           << "nwh=" << nwh << "ndt=" << ndt << "size=" << width << "x"
           << height << "subsystem=" << static_cast<int>(subsystem);
}

void VideoRenderWorker::requestFrameRender(int64_t frameNumber) {
  QMutexLocker lock(&m_mutex);
  m_requestedFrame = frameNumber;
  m_condition.wakeAll();
}

void VideoRenderWorker::requestResize(int width, int height) {
  QMutexLocker lock(&m_mutex);
  m_pendingWidth = width;
  m_pendingHeight = height;
  m_pendingResize = true;
  m_condition.wakeAll();
}

// ── bgfx 初期化 ─────────────────────────────────────────────────────────────
void VideoRenderWorker::initBgfx() {
  qDebug() << "[VideoRenderWorker::initBgfx] thread="
           << QThread::currentThreadId();

  if (!m_nwh) {
    qCritical() << "[VideoRenderWorker::initBgfx] FATAL: nwh is null!";
    m_bgfxReady = false;
    return;
  }

  bgfx::PlatformData pd{};
  pd.nwh = m_nwh;

#if defined(SDL_PLATFORM_LINUX)
  if (m_subsystem == WindowSubsystem::Wayland) {
    pd.ndt = m_ndt;
    pd.type = bgfx::NativeWindowHandleType::Wayland;
    qDebug() << "[VideoRenderWorker::initBgfx] NativeWindowHandleType::Wayland";
  } else {
    pd.ndt = m_ndt;
    pd.type = bgfx::NativeWindowHandleType::Default; // Xlib
    qDebug() << "[VideoRenderWorker::initBgfx] NativeWindowHandleType::Default "
                "(X11)";
  }
#elif defined(SDL_PLATFORM_MACOS)
  pd.ndt = nullptr;
  pd.type = bgfx::NativeWindowHandleType::Default;
#else
  pd.ndt = nullptr;
  pd.type = bgfx::NativeWindowHandleType::Default;
#endif

  bgfx::Init init{};
  init.platformData = pd;

#if defined(SDL_PLATFORM_MACOS)
  init.type = bgfx::RendererType::Metal;
#else
  init.type = bgfx::RendererType::Vulkan;
#endif
  init.resolution.width = static_cast<uint32_t>(m_width);
  init.resolution.height = static_cast<uint32_t>(m_height);
  init.resolution.reset = BGFX_RESET_VSYNC;

  qDebug() << "[VideoRenderWorker::initBgfx] calling bgfx::init()"
           << bgfx::getRendererName(init.type) << m_width << "x" << m_height;

  if (!bgfx::init(init)) {
    qCritical() << "[VideoRenderWorker::initBgfx] FATAL: bgfx::init() failed!";
    m_bgfxReady = false;
    return;
  }

  qDebug() << "[VideoRenderWorker::initBgfx] success:"
           << bgfx::getRendererName(bgfx::getRendererType());
  m_bgfxReady = true;
}

void VideoRenderWorker::shutdownBgfx() {
  if (m_bgfxReady) {
    bgfx::shutdown();
    m_bgfxReady = false;
    qDebug() << "[VideoRenderWorker::shutdownBgfx] done";
  }
}

// ── メインレンダーループ ─────────────────────────────────────────────────────
void VideoRenderWorker::run() {
  qDebug() << "[VideoRenderWorker::run] started, id="
           << QThread::currentThreadId();

  initBgfx();
  if (!m_bgfxReady) {
    qCritical() << "[VideoRenderWorker::run] bgfx init failed; exiting.";
    return;
  }

  while (m_running) {
    int64_t frameToRender = -1;
    bool doResize = false;
    int newW = 0, newH = 0;

    {
      QMutexLocker lock(&m_mutex);
      while (m_running && m_requestedFrame == -1 && !m_pendingResize)
        m_condition.wait(&m_mutex);

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
      }
    }

    // リサイズ処理 — bgfx::reset はレンダースレッド内から呼ぶ
    if (doResize) {
      bgfx::reset(static_cast<uint32_t>(newW), static_cast<uint32_t>(newH),
                  BGFX_RESET_VSYNC);
      m_width = newW;
      m_height = newH;
      qDebug() << "[VideoRenderWorker::run] bgfx::reset" << newW << "x" << newH;

      // リサイズ後は即座に再描画する
      frameToRender = m_frameCounter;
    }

    if (frameToRender < 0)
      continue;

    // フレーム描画 (動作確認: 虹色クリア)
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
  qDebug() << "[VideoRenderWorker::run] exiting";
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  PreviewHostSdl
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
PreviewHostSdl::PreviewHostSdl(QObject *parent) : QObject(parent) {}

PreviewHostSdl::~PreviewHostSdl() { destroy(); }

// ── OS 別 native handle 取得 (SDL3 Property API) ────────────────────────────
bool PreviewHostSdl::getNativeHandles(
    SDL_Window *window, void **outNwh, void **outNdt,
    VideoRenderWorker::WindowSubsystem *outSubsystem) {
  *outNwh = nullptr;
  *outNdt = nullptr;
  *outSubsystem = VideoRenderWorker::WindowSubsystem::Unknown;

  SDL_PropertiesID props = SDL_GetWindowProperties(window);
  if (!props) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "[PreviewHostSdl] SDL_GetWindowProperties failed: %s",
                 SDL_GetError());
    return false;
  }

#if defined(SDL_PLATFORM_WIN32)
  *outNwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                                   nullptr);
  *outNdt = nullptr;
  *outSubsystem = VideoRenderWorker::WindowSubsystem::Win32;

#elif defined(SDL_PLATFORM_MACOS)
  SDL_MetalView metalView = SDL_Metal_CreateView(window);
  if (!metalView) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "[PreviewHostSdl] SDL_Metal_CreateView failed: %s",
                 SDL_GetError());
    return false;
  }
  *outNwh = SDL_Metal_GetLayer(metalView);
  *outNdt = nullptr;
  *outSubsystem = VideoRenderWorker::WindowSubsystem::Metal;

#elif defined(SDL_PLATFORM_LINUX)
  const char *driver = SDL_GetCurrentVideoDriver();
  if (SDL_strcmp(driver, "x11") == 0) {
    *outNdt = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER,
                                     nullptr);
    uintptr_t xwin = static_cast<uintptr_t>(
        SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
    *outNwh = reinterpret_cast<void *>(xwin);
    *outSubsystem = VideoRenderWorker::WindowSubsystem::X11;
    qDebug() << "[PreviewHostSdl] Linux/X11 display=" << *outNdt
             << "window=" << *outNwh;
  } else if (SDL_strcmp(driver, "wayland") == 0) {
    *outNdt = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    *outNwh = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    *outSubsystem = VideoRenderWorker::WindowSubsystem::Wayland;
    qDebug() << "[PreviewHostSdl] Linux/Wayland display=" << *outNdt
             << "surface=" << *outNwh;
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "[PreviewHostSdl] Unsupported video driver: %s", driver);
    return false;
  }
#else
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
               "[PreviewHostSdl] Unsupported platform");
  return false;
#endif

  if (!*outNwh) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "[PreviewHostSdl] nwh is null after handle extraction");
    return false;
  }
  return true;
}

// ── create ───────────────────────────────────────────────────────────────────
bool PreviewHostSdl::create(const char *title, int width, int height) {
  qDebug() << "[PreviewHostSdl::create]" << title << width << "x" << height;

  // [変更] SDL_WINDOW_RESIZABLE フラグを付けてリサイズ可能にする。
  // bgfx は surface を自前で管理するため SDL_WINDOW_VULKAN は付けない。
  m_sdlWindow = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);
  if (!m_sdlWindow) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "[PreviewHostSdl::create] SDL_CreateWindow failed: %s",
                 SDL_GetError());
    return false;
  }

  void *nwh = nullptr, *ndt = nullptr;
  VideoRenderWorker::WindowSubsystem subsystem;
  if (!getNativeHandles(m_sdlWindow, &nwh, &ndt, &subsystem)) {
    SDL_DestroyWindow(m_sdlWindow);
    m_sdlWindow = nullptr;
    return false;
  }

  m_renderWorker = new VideoRenderWorker(this);
  m_renderWorker->setNativeWindowInfo(nwh, ndt, width, height, subsystem);
  m_renderWorker->start();
  m_workerStarted = true;

  // 最初のフレームを即時リクエスト
  m_renderWorker->requestFrameRender(0);

  qDebug() << "[PreviewHostSdl::create] SDL window created, renderer started";
  return true;
}

// ── destroy ──────────────────────────────────────────────────────────────────
void PreviewHostSdl::destroy() {
  if (m_renderWorker) {
    delete m_renderWorker;
    m_renderWorker = nullptr;
    m_workerStarted = false;
  }
  if (m_sdlWindow) {
    SDL_DestroyWindow(m_sdlWindow);
    m_sdlWindow = nullptr;
  }
  qDebug() << "[PreviewHostSdl::destroy] done";
}

// ── pollEvents ───────────────────────────────────────────────────────────────
void PreviewHostSdl::pollEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {

    case SDL_EVENT_QUIT:
      emit windowCloseRequested();
      break;

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      emit windowCloseRequested();
      break;

    // [変更] SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED を使う。
    //
    // SDL3 のリサイズイベントは 2 種類ある:
    //   SDL_EVENT_WINDOW_RESIZED          … 論理サイズ変化 (DPI非考慮)
    //   SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED … ピクセルサイズ変化 (HiDPI 考慮)
    //
    // bgfx のバックバッファは物理ピクセル単位で管理されるため、
    // PIXEL_SIZE_CHANGED を優先して使用する。
    // 非 HiDPI 環境では両イベントが同時に発火するが、
    // PIXEL_SIZE_CHANGED のみを処理することで二重呼び出しを防ぐ。
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
      if (!m_workerStarted || !m_renderWorker)
        break;
      // SDL3: event.window.data1 / data2 は int 型 (SDL_WindowEvent)
      int w = event.window.data1;
      int h = event.window.data2;
      if (w > 0 && h > 0) {
        qDebug() << "[PreviewHostSdl] SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED" << w
                 << "x" << h;
        m_renderWorker->requestResize(w, h);
      }
      break;
    }

    default:
      break;
    }
  }
}

// ── seekToFrame ──────────────────────────────────────────────────────────────
void PreviewHostSdl::seekToFrame(int64_t frameNumber) {
  if (m_workerStarted && m_renderWorker)
    m_renderWorker->requestFrameRender(frameNumber);
}

} // namespace System
} // namespace AviQtl
