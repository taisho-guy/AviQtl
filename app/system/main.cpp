#include "window_manager/window_manager.hpp"
#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QSplashScreen>
#include <QTimer>
#include <QTranslator>
#include <SDL3/SDL.h>

inline void initMyResources() { Q_INIT_RESOURCE(resources); }

int main(int argc, char *argv[]) {
  // SDL3 はアプリ起動直後、Qt より先に初期化する。
  // SDL_INIT_VIDEO のみ: bgfx が Vulkan / Metal ホストとして使う。
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[main] SDL_Init failed: %s",
                 SDL_GetError());
    return 1;
  }

  QApplication app(argc, argv);
  initMyResources();

  // [変更] SDL ウィンドウがメインウィンドウになったため、
  // Qt の最後のウィンドウが閉じられてもアプリを終了しない。
  // 終了トリガーは SDL の SDL_EVENT_WINDOW_CLOSE_REQUESTED →
  // QApplication::quit()。
  app.setQuitOnLastWindowClosed(false);
  QApplication::setApplicationName(QStringLiteral("AviQtl"));
  app.setWindowIcon(QIcon(QStringLiteral(":/assets/icon.svg")));

  const QString appDir = QApplication::applicationDirPath();
  QString resourceDir =
      QDir(appDir + QStringLiteral("/../Resources")).canonicalPath();
  if (resourceDir.isEmpty())
    resourceDir = appDir;

  QTranslator translator;
  if (translator.load(QLocale::system(), QStringLiteral("AviQtl"),
                      QStringLiteral("_"),
                      resourceDir + QStringLiteral("/i18n")))
    app.installTranslator(&translator);

  QSplashScreen splash(
      QIcon(QStringLiteral(":/assets/splash.svg")).pixmap(128, 128));
  splash.show();

  QTimer::singleShot(500, [&]() {
    AviQtl::System::WindowManager::instance().spawnInitialWindows();
    // [変更] メインウィンドウは SDL ウィンドウのため nullptr を渡す。
    // nullptr を渡すと QSplashScreen は即座に非表示になる。
    splash.finish(nullptr);
  });

  int ret = app.exec();

  // Qt イベントループ終了後に SDL をクリーンアップ
  AviQtl::System::WindowManager::instance().shutdown();
  SDL_Quit();

  return ret;
}
