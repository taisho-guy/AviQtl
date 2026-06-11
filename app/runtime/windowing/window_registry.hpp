#pragma once

#include <QMainWindow>
#include <QObject>
#include <QPointer>

#include "sdl/sdl_preview_surface.hpp"

namespace AviQtl {
namespace Runtime {

class WindowRegistry : public QObject {
  Q_OBJECT
public:
  static WindowRegistry &instance();

  void registerSettingsWindow(QMainWindow *window);
  void registerTimelineWindow(QMainWindow *window);
  void registerPreviewSurface(AviQtl::Platform::SdlPreviewSurface *surface);

  QMainWindow *getSettingsWindow() const { return m_settingsWindow.data(); }
  QMainWindow *getTimelineWindow() const { return m_timelineWindow.data(); }
  AviQtl::Platform::SdlPreviewSurface *getPreviewSurface() const {
    return m_previewSurface.data();
  }

private:
  explicit WindowRegistry(QObject *parent = nullptr);
  ~WindowRegistry() override;

  WindowRegistry(const WindowRegistry &) = delete;
  WindowRegistry &operator=(const WindowRegistry &) = delete;

  QPointer<QMainWindow> m_settingsWindow;
  QPointer<QMainWindow> m_timelineWindow;
  QPointer<AviQtl::Platform::SdlPreviewSurface> m_previewSurface;
};

} // namespace Runtime
} // namespace AviQtl
