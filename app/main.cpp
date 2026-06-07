#include <QApplication>
#include <QEvent>
#include <QMainWindow>
#include <QMutex>
#include <QMutexLocker>
#include <QRhiWidget>
#include <QSlider>
#include <QThread>
#include <QVBoxLayout>
#include <QWaitCondition>
#include <atomic>
#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <rhi/qrhi.h>

static const uint8_t s_dummy_vs[] = {0};
static const uint8_t s_dummy_fs[] = {0};

static const bgfx::EmbeddedShader s_embeddedShaders[] = {
    {"vs_video", bgfx::RendererType::Noop, s_dummy_vs},
    {"fs_video", bgfx::RendererType::Noop, s_dummy_fs},
    {nullptr, bgfx::RendererType::Count, nullptr}};

// 1. スレッド間通信用：描画完了イベント
class BgfxFrameReadyEvent : public QEvent {
public:
  static const QEvent::Type EventType =
      static_cast<QEvent::Type>(QEvent::User + 1);
  explicit BgfxFrameReadyEvent(int bufferIdx)
      : QEvent(EventType), m_bufferIdx(bufferIdx) {}
  int readyBufferIndex() const { return m_bufferIdx; }

private:
  int m_bufferIdx;
};

// 2. 非同期レンダーコア（bgfxスレッド）
class VideoRenderWorker : public QThread {
  Q_OBJECT
public:
  VideoRenderWorker(QObject *widgetAddress)
      : m_widgetAddress(widgetAddress), m_running(true), m_requestedFrame(-1),
        m_targetBufferIdx(0) {}

  ~VideoRenderWorker() {
    m_running = false;
    m_condition.wakeAll();
    wait();
  }

  void requestFrameRender(int64_t frameNumber, int bufferIdx,
                          void *nativeTextures[2]) {
    QMutexLocker lock(&m_mutex);
    m_requestedFrame = frameNumber;
    m_targetBufferIdx = bufferIdx;
    m_nativeTextures[0] = nativeTextures[0];
    m_nativeTextures[1] = nativeTextures[1];
    m_condition.wakeAll();
  }

protected:
  void run() override {
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
        bgfx::setViewRect(0, 0, 0, 1920, 1080);
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x1a1a1aff,
                           1.0f, 0);
        bgfx::touch(0);
        bgfx::frame();
        QCoreApplication::postEvent(m_widgetAddress,
                                    new BgfxFrameReadyEvent(bufferIdx));
      }
    }
  }

private:
  QObject *m_widgetAddress;
  std::atomic<bool> m_running;
  QMutex m_mutex;
  QWaitCondition m_condition;
  int64_t m_requestedFrame;
  int m_targetBufferIdx;
  void *m_nativeTextures[2] = {nullptr, nullptr};
};

// 3. プレビュー用カスタムウィジェット
class VideoEditorPreviewWidget : public QRhiWidget {
  Q_OBJECT
public:
  explicit VideoEditorPreviewWidget(QWidget *parent = nullptr)
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

  ~VideoEditorPreviewWidget() { delete m_renderWorker; }

  void seekToFrame(int64_t frameNumber) {
    int nextBackBufferIdx = 1 - m_currentFrontBufferIdx;
    m_renderWorker->requestFrameRender(frameNumber, nextBackBufferIdx,
                                       m_nativeTextures);
  }

protected:
  void initialize(QRhiCommandBuffer *cb) override { Q_UNUSED(cb); }

  void render(QRhiCommandBuffer *cb) override {
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

  bool event(QEvent *event) override {
    if (event->type() == BgfxFrameReadyEvent::EventType) {
      auto *typedEvent = static_cast<BgfxFrameReadyEvent *>(event);
      m_currentFrontBufferIdx = typedEvent->readyBufferIndex();
      update();
      return true;
    }
    return QRhiWidget::event(event);
  }

private:
  VideoRenderWorker *m_renderWorker = nullptr;
  int m_currentFrontBufferIdx = 0;
  void *m_nativeTextures[2] = {nullptr, nullptr};
};

// 4. アプリケーションエントリー
int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  QMainWindow mainWindow;
  mainWindow.setWindowTitle("BgfxUtl - Video Editor Engine (Vulkan/Metal)");
  mainWindow.resize(1280, 720);

  QWidget *centralWidget = new QWidget(&mainWindow);
  QVBoxLayout *layout = new QVBoxLayout(centralWidget);

  VideoEditorPreviewWidget *previewWidget = new VideoEditorPreviewWidget();
  layout->addWidget(previewWidget);

  QSlider *timelineSlider = new QSlider(Qt::Horizontal);
  timelineSlider->setRange(0, 1000);
  layout->addWidget(timelineSlider);

  QObject::connect(timelineSlider, &QSlider::valueChanged,
                   [previewWidget](int value) {
                     previewWidget->seekToFrame(static_cast<int64_t>(value));
                   });

  mainWindow.setCentralWidget(centralWidget);
  mainWindow.show();

  return app.exec();
}

#include "main.moc"