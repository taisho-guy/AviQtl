#pragma once

#include "preview/preview_render_thread.hpp"
#include <QObject>

struct SDL_Window;

namespace AviQtl {
namespace Platform {

class SdlPreviewSurface : public QObject {
  Q_OBJECT
public:
  explicit SdlPreviewSurface(QObject *parent = nullptr);
  ~SdlPreviewSurface() override;

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
  AviQtl::Render::PreviewRenderThread *m_renderThread{nullptr};
  bool m_workerStarted{false};

  // SDL プロパティから OS 別ネイティブハンドルを取得
  static bool getNativeHandles(
      SDL_Window *window, void **outNwh, void **outNdt,
      AviQtl::Render::PreviewRenderThread::WindowSubsystem *outSubsystem);
};

} // namespace Platform
} // namespace AviQtl
