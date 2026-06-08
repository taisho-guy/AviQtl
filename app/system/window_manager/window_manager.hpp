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

  // Qt ウィンドウ群と SDL プレビューウィンドウを生成する
  void spawnInitialWindows();

  // SDL プレビューの破棄 (main() の app.exec() 終了後に呼ぶ)
  void shutdown();

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
