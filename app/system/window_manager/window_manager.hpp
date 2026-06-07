#pragma once

#include <QMainWindow>
#include <QObject>
#include <QPointer>

namespace AviQtl {
namespace System {

class WindowManager : public QObject {
  Q_OBJECT
public:
  static WindowManager &instance();

  void spawnInitialWindows();

  QMainWindow *getMainWindow() const { return m_mainWindow.data(); }
  QMainWindow *getSettingsWindow() const { return m_settingsWindow.data(); }
  QMainWindow *getTimelineWindow() const { return m_timelineWindow.data(); }

private:
  explicit WindowManager(QObject *parent = nullptr);
  ~WindowManager() override;

  WindowManager(const WindowManager &) = delete;
  WindowManager &operator=(const WindowManager &) = delete;

  QPointer<QMainWindow> m_mainWindow;
  QPointer<QMainWindow> m_settingsWindow;
  QPointer<QMainWindow> m_timelineWindow;
};

} // namespace System
} // namespace AviQtl