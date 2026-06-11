#include "windowing/window_coordinator.hpp"
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
  // Linux/Wayland 環境では DISPLAY (XWayland) が存在していても SDL を強制的に
  // Wayland ドライバで動かす。こうしないと bgfx の Vulkan バックエンドが
  // DISPLAY 変数を見て Xlib surface 生成を試みて SIGSEGV する。
  // 対策: WAYLAND_DISPLAY が存在する場合は DISPLAY を unset し、
  //       SDL_VIDEODRIVER を wayland に固定する。
#if defined(SDL_PLATFORM_LINUX)
  {
    SDL_Environment *env = SDL_GetEnvironment();
    if (SDL_GetEnvironmentVariable(env, "WAYLAND_DISPLAY")) {
      SDL_SetEnvironmentVariable(env, "SDL_VIDEODRIVER", "wayland", true);
      SDL_UnsetEnvironmentVariable(env, "DISPLAY");
    }
  }
#endif

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
    AviQtl::Runtime::WindowCoordinator::instance().spawnInitialWindows();
    splash.finish(nullptr);
  });

  int ret = app.exec();

  // Qt イベントループ終了後に SDL をクリーンアップ
  AviQtl::Runtime::WindowCoordinator::instance().shutdown();
  SDL_Quit();

  return ret;
}
