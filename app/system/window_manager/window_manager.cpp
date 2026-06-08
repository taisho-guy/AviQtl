#include "window_manager.hpp"
#include "../../ui/preview/preview.hpp"
#include "../../ui/settings/settings.hpp"
#include "../../ui/timeline/timeline.hpp"
#include "../preview/preview.hpp"

#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace AviQtl {
namespace System {

// ─────────────────────────────────────────────
//  内部実装クラス
//  WindowManager の .cpp 内に閉じ込め、
//  ヘッダに SDL / bgfx の型を漏らさない。
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

  if (m_mainWindow || m_settingsWindow || m_timelineWindow) {
    qWarning() << "[WindowManager::spawnInitialWindows] already spawned, skip";
    return;
  }

  // ── 1. SDL プレビューウィンドウ ──────────────────────────────────────
  //  PreviewHostSdl は Qt オブジェクトとして this の子にする。
  //  これにより Qt の親子関係でライフタイムが管理される。
  auto *previewHost = new PreviewHostSdl(this);

  if (!previewHost->create("AviQtl - Preview", 1280, 720)) {
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

  // ── 2. Qt メインウィンドウ (タイムライン / 設定 等の編集 UI) ─────────
  m_mainWindow = new QMainWindow();
  m_mainWindow->setWindowTitle(QStringLiteral("AviQtl"));
  m_mainWindow->resize(800, 600);
  qDebug() << "[WindowManager] Qt mainWindow created";

  // UI レイヤー: VideoEditorPreviewWidget はシーク要求を中継するだけ
  auto *centralWidget = new QWidget(m_mainWindow.data());
  auto *centralLayout = new QVBoxLayout(centralWidget);
  centralLayout->setContentsMargins(0, 0, 0, 0);

  auto *uiPreview = new UI::VideoEditorPreviewWidget(centralWidget);
  centralLayout->addWidget(uiPreview);
  m_mainWindow->setCentralWidget(centralWidget);
  qDebug() << "[WindowManager] UI::VideoEditorPreviewWidget created";

  m_mainWindow->show();
  qDebug() << "[WindowManager] Qt mainWindow shown";

  // ── 3. 設定ウィンドウ ────────────────────────────────────────────────
  m_settingsWindow = new QMainWindow();
  m_settingsWindow->setWindowTitle(QStringLiteral("AviQtl - Settings"));
  m_settingsWindow->resize(400, 300);
  m_settingsWindow->move(m_mainWindow->x() + 50, m_mainWindow->y() + 50);
  auto *settingsWidget = new UI::SettingsDialogWidget(m_settingsWindow.data());
  m_settingsWindow->setCentralWidget(settingsWidget);
  m_settingsWindow->show();
  qDebug() << "[WindowManager] settingsWindow shown";

  // ── 4. タイムラインウィンドウ ─────────────────────────────────────────
  m_timelineWindow = new QMainWindow();
  m_timelineWindow->setWindowTitle(QStringLiteral("AviQtl - Timeline"));
  m_timelineWindow->resize(800, 250);
  m_timelineWindow->move(m_mainWindow->x(),
                         m_mainWindow->y() + m_mainWindow->height() + 40);
  auto *timelineWidget = new UI::TimelineWidget(m_timelineWindow.data());
  m_timelineWindow->setCentralWidget(timelineWidget);
  m_timelineWindow->show();
  qDebug() << "[WindowManager] timelineWindow shown";

  // ── シグナル接続 ──────────────────────────────────────────────────────
  //  timeline ──frameChanged──► uiPreview ──requestSeek──► previewHost
  //
  //  Qt の編集 UI ↔ SDL プレビューの境界は
  //  frameChanged(int64_t) / requestSeek(int64_t) のみ。
  connect(timelineWidget, &UI::TimelineWidget::frameChanged, uiPreview,
          &UI::VideoEditorPreviewWidget::seekToFrame);
  connect(uiPreview, &UI::VideoEditorPreviewWidget::requestSeek, previewHost,
          &PreviewHostSdl::seekToFrame);

  qDebug() << "[WindowManager::spawnInitialWindows] all windows spawned";
}

// ─────────────────────────────────────────────
//  shutdown
// ─────────────────────────────────────────────
void WindowManager::shutdown() {
  // PreviewHostSdl は this の子なので、
  // findChild で取得してから明示的に destroy() を呼ぶ。
  // デストラクタでも destroy() は呼ばれるが、
  // SDL_Quit() の前に確実に破棄したいため手動で行う。
  auto *previewHost = findChild<PreviewHostSdl *>();
  if (previewHost)
    previewHost->destroy();

  qDebug() << "[WindowManager::shutdown] done";
}

} // namespace System
} // namespace AviQtl
