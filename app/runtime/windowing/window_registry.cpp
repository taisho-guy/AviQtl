#include "window_registry.hpp"
#include "sdl/sdl_preview_surface.hpp"

namespace AviQtl {
namespace Runtime {

WindowRegistry &WindowRegistry::instance() {
  static WindowRegistry inst;
  return inst;
}

WindowRegistry::WindowRegistry(QObject *parent) : QObject(parent) {}

WindowRegistry::~WindowRegistry() {}

void WindowRegistry::registerSettingsWindow(QMainWindow *window) {
  m_settingsWindow = window;
}

void WindowRegistry::registerTimelineWindow(QMainWindow *window) {
  m_timelineWindow = window;
}

void WindowRegistry::registerPreviewSurface(
    AviQtl::Platform::SdlPreviewSurface *surface) {
  m_previewSurface = surface;
}

} // namespace Runtime
} // namespace AviQtl
