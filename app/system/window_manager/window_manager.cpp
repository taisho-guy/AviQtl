#include "window_manager.hpp"
#include "../../ui/settings/settings.hpp"
#include "../../ui/timeline/timeline.hpp"
#include "../preview/preview.hpp"

#include <QApplication>
#include <QDebug>
#include <QTimer>

namespace AviQtl {
namespace System {

// ─────────────────────────────────────────────
//  定数
// ─────────────────────────────────────────────
namespace {

// SDL イベントポーリング間隔 (ms)
// 16ms ≒ 60fps ポーリング。入力遅延とバッテリ消費のバランス点。
constexpr int kSdlPollIntervalMs = 16;

} // anonymous namespace

// ─────────────────────────────────────────────
//  Singleton
// ─────────────────────────────────────────────
WindowManager &WindowManager::instance() {
  static WindowManager inst;
  return inst;
}

WindowManager::WindowManager(QObject *parent) : QObject(parent) {
  qDebug() << "[WindowManager] constructed";
}

WindowManager::~WindowManager() { qDebug() << "[WindowManager] destructor"; }

// ─────────────────────────────────────────────
//  spawnInitialWindows
// ─────────────────────────────────────────────
void WindowManager::spawnInitialWindows() {
  qDebug() << "[WindowManager::spawnInitialWindows] called";

  if (m_settingsWindow || m_timelineWindow) {
    qWarning() << "[WindowManager::spawnInitialWindows] already spawned, skip";
    return;
  }

  // ── 1. SDL プレビューウィンドウ (= メインウィンドウ) ─────────────────
  //  PreviewHostSdl は Qt オブジェクトとして this の子にする。
  //  Qt の親子関係でライフタイムが管理される。
  auto *previewHost = new PreviewHostSdl(this);

  if (!previewHost->create("AviQtl", 1280, 720)) {
    qCritical() << "[WindowManager] FATAL: PreviewHostSdl::create() failed!";
    delete previewHost;
    return;
  }
  qDebug() << "[WindowManager] SDL preview window created";

  // SDL イベントを Qt タイマーでポーリング
  auto *sdlPollTimer = new QTimer(this);
  sdlPollTimer->setInterval(kSdlPollIntervalMs);
  connect(sdlPollTimer, &QTimer::timeout, previewHost,
          &PreviewHostSdl::pollEvents);
  sdlPollTimer->start();

  // SDL ウィンドウが閉じられたらアプリを終了
  connect(previewHost, &PreviewHostSdl::windowCloseRequested, qApp,
          &QApplication::quit);

  // ── 2. 設定ウィンドウ (Qt トップレベル) ─────────────────────────────
  m_settingsWindow = new QMainWindow();
  m_settingsWindow->setWindowTitle(QStringLiteral("AviQtl - Settings"));
  m_settingsWindow->resize(400, 300);
  auto *settingsWidget = new UI::SettingsDialogWidget(m_settingsWindow.data());
  m_settingsWindow->setCentralWidget(settingsWidget);
  m_settingsWindow->show();
  qDebug() << "[WindowManager] settingsWindow shown";

  // ── 3. タイムラインウィンドウ (Qt トップレベル) ──────────────────────
  m_timelineWindow = new QMainWindow();
  m_timelineWindow->setWindowTitle(QStringLiteral("AviQtl - Timeline"));
  m_timelineWindow->resize(800, 250);
  auto *timelineWidget = new UI::TimelineWidget(m_timelineWindow.data());
  m_timelineWindow->setCentralWidget(timelineWidget);
  m_timelineWindow->show();
  qDebug() << "[WindowManager] timelineWindow shown";

  // ── シグナル接続 ──────────────────────────────────────────────────────
  //  [変更] 中継役の VideoEditorPreviewWidget を撤廃し、
  //  タイムラインから PreviewHostSdl へ直結する。
  //  Qt 編集 UI ↔ SDL プレビューの境界は frameChanged(int64_t) のみ。
  connect(timelineWidget, &UI::TimelineWidget::frameChanged, previewHost,
          &PreviewHostSdl::seekToFrame);

  qDebug() << "[WindowManager::spawnInitialWindows] all windows spawned";
}

// ─────────────────────────────────────────────
//  shutdown
// ─────────────────────────────────────────────
void WindowManager::shutdown() {
  // PreviewHostSdl は this の子なので findChild で取得し、
  // SDL_Quit() の前に確実に destroy() する。
  auto *previewHost = findChild<PreviewHostSdl *>();
  if (previewHost)
    previewHost->destroy();

  qDebug() << "[WindowManager::shutdown] done";
}

} // namespace System
} // namespace AviQtl
