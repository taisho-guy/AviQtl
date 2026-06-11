#include "sdl_preview_surface.hpp"

#include <QDebug>
#include <SDL3/SDL.h>

#if defined(SDL_PLATFORM_MACOS)
#include <SDL3/SDL_metal.h>
#endif

namespace AviQtl {
namespace Platform {

SdlPreviewSurface::SdlPreviewSurface(QObject *parent) : QObject(parent) {}

SdlPreviewSurface::~SdlPreviewSurface() { destroy(); }

bool SdlPreviewSurface::getNativeHandles(
    SDL_Window *window, void **outNwh, void **outNdt,
    AviQtl::Render::PreviewRenderThread::WindowSubsystem *outSubsystem) {
  *outNwh = nullptr;
  *outNdt = nullptr;
  *outSubsystem = AviQtl::Render::PreviewRenderThread::WindowSubsystem::Unknown;

  SDL_PropertiesID props = SDL_GetWindowProperties(window);
  if (!props) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "[SdlPreviewSurface] SDL_GetWindowProperties failed: %s",
                 SDL_GetError());
    return false;
  }

#if defined(SDL_PLATFORM_WIN32)
  *outNwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                                   nullptr);
  *outNdt = nullptr;
  *outSubsystem = AviQtl::Render::PreviewRenderThread::WindowSubsystem::Win32;

#elif defined(SDL_PLATFORM_MACOS)
  SDL_MetalView metalView = SDL_Metal_CreateView(window);
  if (!metalView) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "[SdlPreviewSurface] SDL_Metal_CreateView failed: %s",
                 SDL_GetError());
    return false;
  }
  *outNwh = SDL_Metal_GetLayer(metalView);
  *outNdt = nullptr;
  *outSubsystem = AviQtl::Render::PreviewRenderThread::WindowSubsystem::Metal;

#elif defined(SDL_PLATFORM_LINUX)
  const char *driver = SDL_GetCurrentVideoDriver();
  if (SDL_strcmp(driver, "x11") == 0) {
    *outNdt = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER,
                                     nullptr);
    uintptr_t xwin = static_cast<uintptr_t>(
        SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
    *outNwh = reinterpret_cast<void *>(xwin);
    *outSubsystem = AviQtl::Render::PreviewRenderThread::WindowSubsystem::X11;
    qDebug() << "[SdlPreviewSurface] Linux/X11 display=" << *outNdt
             << "window=" << *outNwh;
  } else if (SDL_strcmp(driver, "wayland") == 0) {
    *outNdt = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    *outNwh = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    *outSubsystem =
        AviQtl::Render::PreviewRenderThread::WindowSubsystem::Wayland;
    qDebug() << "[SdlPreviewSurface] Linux/Wayland display=" << *outNdt
             << "surface=" << *outNwh;
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "[SdlPreviewSurface] Unsupported video driver: %s", driver);
    return false;
  }
#else
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
               "[SdlPreviewSurface] Unsupported platform");
  return false;
#endif

  if (!*outNwh) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "[SdlPreviewSurface] nwh is null after handle extraction");
    return false;
  }
  return true;
}

bool SdlPreviewSurface::create(const char *title, int width, int height) {
  qDebug() << "[SdlPreviewSurface::create]" << title << width << "x" << height;

  m_sdlWindow = SDL_CreateWindow(title, width, height,
                                 SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
  if (!m_sdlWindow) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "[SdlPreviewSurface::create] SDL_CreateWindow failed: %s",
                 SDL_GetError());
    return false;
  }

  void *nwh = nullptr, *ndt = nullptr;
  AviQtl::Render::PreviewRenderThread::WindowSubsystem subsystem;
  if (!getNativeHandles(m_sdlWindow, &nwh, &ndt, &subsystem)) {
    SDL_DestroyWindow(m_sdlWindow);
    m_sdlWindow = nullptr;
    return false;
  }

  m_renderThread = new AviQtl::Render::PreviewRenderThread(this);
  m_renderThread->setNativeWindowInfo(nwh, ndt, width, height, subsystem);
  m_renderThread->start();
  m_workerStarted = true;

  m_renderThread->requestFrameRender(0);

  qDebug()
      << "[SdlPreviewSurface::create] SDL window created, renderer started";
  return true;
}

void SdlPreviewSurface::destroy() {
  if (m_renderThread) {
    delete m_renderThread;
    m_renderThread = nullptr;
    m_workerStarted = false;
  }
  if (m_sdlWindow) {
    SDL_DestroyWindow(m_sdlWindow);
    m_sdlWindow = nullptr;
  }
  qDebug() << "[SdlPreviewSurface::destroy] done";
}

void SdlPreviewSurface::pollEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
      emit windowCloseRequested();
      break;

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      emit windowCloseRequested();
      break;

    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
      if (!m_workerStarted || !m_renderThread)
        break;
      int w = event.window.data1;
      int h = event.window.data2;
      if (w > 0 && h > 0) {
        qDebug() << "[SdlPreviewSurface] SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED"
                 << w << "x" << h;
        m_renderThread->requestResize(w, h);
      }
      break;
    }
    default:
      break;
    }
  }
}

void SdlPreviewSurface::seekToFrame(int64_t frameNumber) {
  if (m_workerStarted && m_renderThread)
    m_renderThread->requestFrameRender(frameNumber);
}

} // namespace Platform
} // namespace AviQtl
