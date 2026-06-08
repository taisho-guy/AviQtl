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

  // [変更] メインウィンドウは SDL ウィンドウになったため nullptr を返す。
  //        main.cpp の splash.finish() は nullptr を渡すと即座に非表示になる
  //        ので実害なし。将来 SDL ウィンドウの QWindow* ラッパーが必要になれば
  //        ここで返す。
  QMainWindow *getMainWindow() const { return nullptr; }
  QMainWindow *getSettingsWindow() const { return m_settingsWindow.data(); }
  QMainWindow *getTimelineWindow() const { return m_timelineWindow.data(); }

private:
  explicit WindowManager(QObject *parent = nullptr);
  ~WindowManager() override;

  WindowManager(const WindowManager &) = delete;
  WindowManager &operator=(const WindowManager &) = delete;

  // [削除] m_mainWindow — SDL ウィンドウがメインウィンドウを兼ねるため不要
  QPointer<QMainWindow> m_settingsWindow;
  QPointer<QMainWindow> m_timelineWindow;
};

} // namespace System
} // namespace AviQtl
