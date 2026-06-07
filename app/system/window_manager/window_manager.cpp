#include "window_manager.hpp"
#include "../../ui/preview/preview.hpp"
#include "../../ui/settings/settings.hpp"
#include "../../ui/timeline/timeline.hpp"
#include "../preview/preview.hpp"
#include <QVBoxLayout>
#include <QWidget>

namespace AviQtl {
namespace System {

WindowManager &WindowManager::instance() {
  static WindowManager inst;
  return inst;
}

WindowManager::WindowManager(QObject *parent) : QObject(parent) {}
WindowManager::~WindowManager() {}

void WindowManager::spawnInitialWindows() {
  if (m_mainWindow || m_settingsWindow || m_timelineWindow)
    return;

  // 1. メインウィンドウ
  m_mainWindow = new QMainWindow();
  m_mainWindow->setWindowTitle(QStringLiteral("AviQtl - Main Preview"));
  m_mainWindow->resize(1280, 720);

  // メインウィンドウの極限まで引き伸ばすための土台 (CentralWidget)
  auto *centralWidget = new QWidget(m_mainWindow.data());
  auto *centralLayout = new QVBoxLayout(centralWidget);
  centralLayout->setContentsMargins(0, 0, 0, 0);

  // UI層のプレビュー器を生成して土台に追加
  auto *uiPreview = new UI::VideoEditorPreviewWidget(centralWidget);
  centralLayout->addWidget(uiPreview);
  m_mainWindow->setCentralWidget(centralWidget);

  // UI層の中に、System層のbgfxレンダラーを埋め込む
  auto *sysRenderer = new SystemPreviewRenderer(uiPreview);
  auto *previewLayout = new QVBoxLayout(uiPreview);
  previewLayout->setContentsMargins(0, 0, 0, 0);
  previewLayout->addWidget(
      sysRenderer);

  m_mainWindow->show();

  // 2. 設定ウィンドウ
  m_settingsWindow = new QMainWindow();
  m_settingsWindow->setWindowTitle(QStringLiteral("AviQtl - Settings"));
  m_settingsWindow->resize(400, 300);
  m_settingsWindow->move(m_mainWindow->x() + 50, m_mainWindow->y() + 50);
  auto *settingsWidget = new UI::SettingsDialogWidget(m_settingsWindow.data());
  m_settingsWindow->setCentralWidget(settingsWidget);
  m_settingsWindow->show();

  // 3. 拡張編集ウィンドウ (タイムライン)
  m_timelineWindow = new QMainWindow();
  m_timelineWindow->setWindowTitle(QStringLiteral("AviQtl - Timeline"));
  m_timelineWindow->resize(800, 250);
  m_timelineWindow->move(m_mainWindow->x(),
                         m_mainWindow->y() + m_mainWindow->height() + 40);
  auto *timelineWidget = new UI::TimelineWidget(m_timelineWindow.data());
  m_timelineWindow->setCentralWidget(timelineWidget);
  m_timelineWindow->show();

  // タイムライン -> UIプレビュー -> Systemレンダラー
  connect(timelineWidget, &UI::TimelineWidget::frameChanged, uiPreview,
          &UI::VideoEditorPreviewWidget::seekToFrame);
  connect(uiPreview, &UI::VideoEditorPreviewWidget::requestSeek, sysRenderer,
          &SystemPreviewRenderer::seekToFrame);
}

} // namespace System
} // namespace AviQtl