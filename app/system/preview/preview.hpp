#pragma once

#include <QEvent>
#include <QMutex>
#include <QRhiWidget>
#include <QThread>
#include <QWaitCondition>
#include <atomic>
#include <cstdint>

namespace AviQtl {
namespace System {

class BgfxFrameReadyEvent : public QEvent {
public:
  static const QEvent::Type EventType;
  explicit BgfxFrameReadyEvent(int bufferIdx);
  int readyBufferIndex() const { return m_bufferIdx; }

private:
  int m_bufferIdx;
};

class VideoRenderWorker : public QThread {
  Q_OBJECT
public:
  explicit VideoRenderWorker(QObject *widgetAddress);
  ~VideoRenderWorker() override;

  void requestFrameRender(int64_t frameNumber, int bufferIdx,
                          void *nativeTextures[2]);

protected:
  void run() override;

private:
  QObject *m_widgetAddress;
  std::atomic<bool> m_running;
  QMutex m_mutex;
  QWaitCondition m_condition;
  int64_t m_requestedFrame;
  int m_targetBufferIdx;
  void *m_nativeTextures[2] = {nullptr, nullptr};
};

// 実際のQRhiWidget+bgfx描画を行うシステムコンポーネント
class SystemPreviewRenderer : public QRhiWidget {
  Q_OBJECT
public:
  explicit SystemPreviewRenderer(QWidget *parent = nullptr);
  ~SystemPreviewRenderer() override;

public slots:
  void seekToFrame(int64_t frameNumber);

protected:
  void initialize(QRhiCommandBuffer *cb) override;
  void render(QRhiCommandBuffer *cb) override;
  bool event(QEvent *event) override;

private:
  VideoRenderWorker *m_renderWorker = nullptr;
  int m_currentFrontBufferIdx = 0;
  void *m_nativeTextures[2] = {nullptr, nullptr};
};

} // namespace System
} // namespace AviQtl