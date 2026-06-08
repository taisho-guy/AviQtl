#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  preview.hpp  —  SDL3 host + bgfx renderer  (preview_host_sdl 層)
//
//  設計概要
//  ┌──────────────┐  preview_protocol  ┌───────────────────────────┐
//  │  Qt Widgets  │ ─────────────────► │  PreviewHostSdl           │
//  │  (UI 層)     │ ◄───────────────── │  SDL ウィンドウ管理        │
//  └──────────────┘   seekToFrame /    │  + VideoRenderWorker      │
//                     inputCommand     │    (bgfx renderer 層)     │
//                                      └───────────────────────────┘
//
//  Qt は SDL / bgfx の型を直接参照しない。
//  PreviewHostSdl が唯一の公開クラス。
// ─────────────────────────────────────────────────────────────────────────────

#include <QMutex>
#include <QObject>
#include <QThread>
#include <QWaitCondition>
#include <atomic>
#include <cstdint>

// 前方宣言 — Qt 側ヘッダに SDL/bgfx の型を露出しない
struct SDL_Window;

namespace AviQtl {
namespace System {

// ─────────────────────────────────────────────
//  VideoRenderWorker  (preview_renderer_bgfx)
//
//  bgfx::init() を呼んだスレッドと同一スレッドで
//  bgfx::frame() まで全操作を行う。
//  SDL ウィンドウから取得したネイティブ情報を受け取り、
//  bgfx を初期化する。
// ─────────────────────────────────────────────
class VideoRenderWorker : public QThread {
  Q_OBJECT
public:
  // [追加] ウィンドウサブシステムの種別
  // bgfx の NativeWindowHandleType に正確にマップするために使用する。
  // Wayland セッションで X11 サーフェス生成を試みると SIGSEGV になるため、
  // X11 と Wayland を明示的に区別することが必須。
  enum class WindowSubsystem {
    Unknown,
    X11,
    Wayland,
    Win32,
    Metal,
  };

  explicit VideoRenderWorker(QObject *parent = nullptr);
  ~VideoRenderWorker() override;

  // SDL ウィンドウから取得したプラットフォーム情報を渡す
  // (スレッド開始前に呼ぶこと)
  // [変更] subsystem 引数を追加 — Wayland/X11 の判別に必要
  void setNativeWindowInfo(void *nwh, void *ndt, int width, int height,
                           WindowSubsystem subsystem);

  // Qt 側からのシーク要求
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
  // [追加] サブシステム種別 — bgfx::NativeWindowHandleType の選択に使用
  WindowSubsystem m_subsystem{WindowSubsystem::Unknown};

  // フレーム/リサイズ要求キュー (単一ミューテックスで保護)
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

// ─────────────────────────────────────────────
//  PreviewHostSdl  (preview_host_sdl)
//
//  SDL3 プレビューウィンドウのライフサイクルを管理する。
//  Qt 側に公開する API は seekToFrame() のみ。
//  SDL イベントループは Qt の QTimer でポーリングする。
// ─────────────────────────────────────────────
class PreviewHostSdl : public QObject {
  Q_OBJECT
public:
  explicit PreviewHostSdl(QObject *parent = nullptr);
  ~PreviewHostSdl() override;

  // ウィンドウ生成 & bgfx 起動
  bool create(const char *title, int width, int height);

  // ウィンドウ破棄 & bgfx シャットダウン
  void destroy();

  // SDL イベントのポーリング (Qt の QTimer で定期呼び出し)
  void pollEvents();

public slots:
  void seekToFrame(int64_t frameNumber);

signals:
  // SDL ウィンドウの閉じるボタンが押された
  void windowCloseRequested();

private:
  SDL_Window *m_sdlWindow{nullptr};
  VideoRenderWorker *m_renderWorker{nullptr};
  bool m_workerStarted{false};

  // SDL プロパティから OS 別ネイティブハンドルを取得
  // [変更] outSubsystem を追加 — Wayland/X11 の判別結果を呼び出し元に返す
  static bool
  getNativeHandles(SDL_Window *window, void **outNwh, void **outNdt,
                   VideoRenderWorker::WindowSubsystem *outSubsystem);
};

} // namespace System
} // namespace AviQtl
