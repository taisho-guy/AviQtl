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
    splash.finish(AviQtl::System::WindowManager::instance().getMainWindow());
  });

  int ret = app.exec();

  // Qt イベントループ終了後に SDL をクリーンアップ
  AviQtl::System::WindowManager::instance().shutdown();
  SDL_Quit();

  return ret;
}
