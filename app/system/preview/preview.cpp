#include "preview.hpp"
#include <QCoreApplication>
#include <bgfx/bgfx.h>
#include <cmath>
#include <rhi/qrhi.h>

namespace AviQtl {
namespace System {

const QEvent::Type BgfxFrameReadyEvent::EventType =
    static_cast<QEvent::Type>(QEvent::User + 1);

BgfxFrameReadyEvent::BgfxFrameReadyEvent(int bufferIdx)
    : QEvent(EventType), m_bufferIdx(bufferIdx) {}

// --- VideoRenderWorker ---
VideoRenderWorker::VideoRenderWorker(QObject *widgetAddress)
    : m_widgetAddress(widgetAddress), m_running(true), m_requestedFrame(-1),
      m_targetBufferIdx(0) {}

VideoRenderWorker::~VideoRenderWorker() {
  m_running = false;
  m_condition.wakeAll();
  wait();
}

void VideoRenderWorker::requestFrameRender(int64_t frameNumber, int bufferIdx,
                                           void *nativeTextures[2]) {
  QMutexLocker lock(&m_mutex);
  m_requestedFrame = frameNumber;
  m_targetBufferIdx = bufferIdx;
  m_nativeTextures[0] = nativeTextures[0];
  m_nativeTextures[1] = nativeTextures[1];
  m_condition.wakeAll();
}

static uint32_t getRainbowColor(float hue) {
  float h = hue * 6.0f;
  int i = static_cast<int>(std::floor(h));
  float f = h - i;
  float q = 1.0f - f;
  float r = 0.0f, g = 0.0f, b = 0.0f;
  switch (i % 6) {
  case 0:
    r = 1.0f;
    g = f;
    b = 0.0f;
    break;
  case 1:
    r = q;
    g = 1.0f;
    b = 0.0f;
    break;
  case 2:
    r = 0.0f;
    g = 1.0f;
    b = f;
    break;
  case 3:
    r = 0.0f;
    g = q;
    b = 1.0f;
    break;
  case 4:
    r = f;
    g = 0.0f;
    b = 1.0f;
    break;
  case 5:
    r = 1.0f;
    g = 0.0f;
    b = q;
    break;
  }
  return (static_cast<uint8_t>(r * 255.0f) << 24) |
         (static_cast<uint8_t>(g * 255.0f) << 16) |
         (static_cast<uint8_t>(b * 255.0f) << 8) | 255;
}

void VideoRenderWorker::run() {
  uint32_t frameCounter = 0;
  while (m_running) {
    int64_t frameToRender = -1;
    int bufferIdx = 0;
    void *currentTexture = nullptr;

    {
      m_mutex.lock();
      while (m_requestedFrame == -1 && m_running) {
        m_condition.wait(&m_mutex);
      }
      if (!m_running) {
        m_mutex.unlock();
        break;
      }
      frameToRender = m_requestedFrame;
      bufferIdx = m_targetBufferIdx;
      currentTexture = m_nativeTextures[bufferIdx];
      m_requestedFrame = -1;
      m_mutex.unlock();
    }

    if (currentTexture) {
      float hue = static_cast<float>(frameCounter % 300) / 300.0f;
      uint32_t clearColor = getRainbowColor(hue);
      frameCounter++;

      bgfx::setViewRect(0, 0, 0, 1920, 1080);
      bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clearColor,
                         1.0f, 0);
      bgfx::touch(0);
      bgfx::frame();
      QCoreApplication::postEvent(m_widgetAddress,
                                  new BgfxFrameReadyEvent(bufferIdx));
    }
  }
}

// --- SystemPreviewRenderer ---
SystemPreviewRenderer::SystemPreviewRenderer(QWidget *parent)
    : QRhiWidget(parent) {
#if defined(Q_OS_WIN)
  setApi(QRhiWidget::Api::Vulkan);
#elif defined(Q_OS_MAC)
  setApi(QRhiWidget::Api::Metal);
#else
  setApi(QRhiWidget::Api::Vulkan);
#endif
  setAutoRenderTarget(false);
  m_renderWorker = new VideoRenderWorker(this);
  m_renderWorker->start();
}

SystemPreviewRenderer::~SystemPreviewRenderer() { delete m_renderWorker; }

void SystemPreviewRenderer::seekToFrame(int64_t frameNumber) {
  int nextBackBufferIdx = 1 - m_currentFrontBufferIdx;
  m_renderWorker->requestFrameRender(frameNumber, nextBackBufferIdx,
                                     m_nativeTextures);
}

void SystemPreviewRenderer::initialize(QRhiCommandBuffer *cb) { Q_UNUSED(cb); }

void SystemPreviewRenderer::render(QRhiCommandBuffer *cb) {
  void *activeFrontHandle = m_nativeTextures[m_currentFrontBufferIdx];
  if (!activeFrontHandle)
    return;

  QRhiTexture *wrappedTexture =
      rhi()->newTexture(QRhiTexture::RGBA8, size() * devicePixelRatio());
  wrappedTexture->createFrom({reinterpret_cast<quintptr>(activeFrontHandle)});
  QRhiResourceUpdateBatch *rub = rhi()->nextResourceUpdateBatch();
  cb->resourceUpdate(rub);
  wrappedTexture->destroy();
}

bool SystemPreviewRenderer::event(QEvent *event) {
  if (event->type() == BgfxFrameReadyEvent::EventType) {
    auto *typedEvent = static_cast<BgfxFrameReadyEvent *>(event);
    m_currentFrontBufferIdx = typedEvent->readyBufferIndex();
    update();
    return true;
  }
  return QRhiWidget::event(event);
}

} // namespace System
} // namespace AviQtl