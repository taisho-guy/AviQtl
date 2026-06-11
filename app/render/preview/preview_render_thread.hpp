#pragma once

#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <atomic>
#include <cstdint>

namespace AviQtl {
namespace Render {

class PreviewRenderThread : public QThread {
  Q_OBJECT
public:
  enum class WindowSubsystem {
    Unknown,
    X11,
    Wayland,
    Win32,
    Metal,
  };

  explicit PreviewRenderThread(QObject *parent = nullptr);
  ~PreviewRenderThread() override;

  // SDL ウィンドウから取得したプラットフォーム情報を渡す
  // (スレッド開始前に呼ぶこと)
  void setNativeWindowInfo(void *nwh, void *ndt, int width, int height,
                           WindowSubsystem subsystem);

  // シーク要求
  void requestFrameRender(int64_t frameNumber);

  // リサイズ要求 (bgfx::reset はレンダースレッド内で実行される)
  void requestResize(int width, int height);

protected:
  void run() override;

private:
  void initBgfx();
  void shutdownBgfx();

  // ネイティブウィンドウ情報
  void *m_nwh{nullptr}; // HWND / NSWindow* / xcb_window_t / wl_surface*
  void *m_ndt{nullptr}; // X11 Display* / wl_display*  (Linux のみ使用)
  int m_width{1280};
  int m_height{720};
  WindowSubsystem m_subsystem{WindowSubsystem::Unknown};

  // 要求キュー
  QMutex m_mutex;
  QWaitCondition m_condition;
  std::atomic<bool> m_running{true};
  std::atomic<bool> m_bgfxReady{false};

  int64_t m_requestedFrame{-1};
  int m_pendingWidth{0};
  int m_pendingHeight{0};
  bool m_pendingResize{false};

  uint32_t m_frameCounter{0};
};

} // namespace Render
} // namespace AviQtl
