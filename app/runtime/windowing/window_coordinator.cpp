#include "window_coordinator.hpp"
#include "sdl/sdl_preview_surface.hpp"
#include "settings/settings.hpp"
#include "timeline/timeline.hpp"
#include "window_registry.hpp"

#include <QApplication>
#include <QDebug>
#include <QMainWindow>
#include <QTimer>

namespace AviQtl {
namespace Runtime {

namespace {
constexpr int kSdlPollIntervalMs = 16;
}

WindowCoordinator &WindowCoordinator::instance() {
  static WindowCoordinator inst;
  return inst;
}

WindowCoordinator::WindowCoordinator(QObject *parent) : QObject(parent) {
  qDebug() << "[WindowCoordinator] constructed";
}

WindowCoordinator::~WindowCoordinator() {
  qDebug() << "[WindowCoordinator] destructor";
}

void WindowCoordinator::spawnInitialWindows() {
  qDebug() << "[WindowCoordinator::spawnInitialWindows] called";

  if (WindowRegistry::instance().getSettingsWindow() ||
      WindowRegistry::instance().getTimelineWindow()) {
    qWarning()
        << "[WindowCoordinator::spawnInitialWindows] already spawned, skip";
    return;
  }

  // 1. SDL プレビューウィンドウ
  auto *previewHost = new Platform::SdlPreviewSurface(this);
  if (!previewHost->create("AviQtl", 1280, 720)) {
    qCritical()
        << "[WindowCoordinator] FATAL: SdlPreviewSurface::create() failed!";
    delete previewHost;
    return;
  }
  WindowRegistry::instance().registerPreviewSurface(previewHost);
  qDebug() << "[WindowCoordinator] SDL preview window created";

  // SDL イベントを Qt タイマーでポーリング
  auto *sdlPollTimer = new QTimer(this);
  sdlPollTimer->setInterval(kSdlPollIntervalMs);
  connect(sdlPollTimer, &QTimer::timeout, previewHost,
          &Platform::SdlPreviewSurface::pollEvents);
  sdlPollTimer->start();

  // SDL ウィンドウが閉じられたらアプリを終了
  connect(previewHost, &Platform::SdlPreviewSurface::windowCloseRequested, qApp,
          &QApplication::quit);

  // 2. 設定ウィンドウ (Qt トップレベル)
  auto *settingsWindow = new QMainWindow();
  settingsWindow->setWindowTitle(QStringLiteral("AviQtl - Settings"));
  settingsWindow->resize(400, 300);
  auto *settingsWidget = new UI::SettingsDialogWidget(settingsWindow);
  settingsWindow->setCentralWidget(settingsWidget);
  settingsWindow->show();
  WindowRegistry::instance().registerSettingsWindow(settingsWindow);
  qDebug() << "[WindowCoordinator] settingsWindow shown";

  // 3. タイムラインウィンドウ (Qt トップレベル)
  auto *timelineWindow = new QMainWindow();
  timelineWindow->setWindowTitle(QStringLiteral("AviQtl - Timeline"));
  timelineWindow->resize(800, 250);
  auto *timelineWidget = new UI::TimelineWidget(timelineWindow);
  timelineWindow->setCentralWidget(timelineWidget);
  timelineWindow->show();
  WindowRegistry::instance().registerTimelineWindow(timelineWindow);
  qDebug() << "[WindowCoordinator] timelineWindow shown";

  // シグナル接続
  connect(timelineWidget, &UI::TimelineWidget::frameChanged, previewHost,
          &Platform::SdlPreviewSurface::seekToFrame);

  qDebug() << "[WindowCoordinator::spawnInitialWindows] all windows spawned";
}

void WindowCoordinator::shutdown() {
  auto *previewHost = findChild<Platform::SdlPreviewSurface *>();
  if (previewHost)
    previewHost->destroy();

  qDebug() << "[WindowCoordinator::shutdown] done";
}

} // namespace Runtime
} // namespace AviQtl
