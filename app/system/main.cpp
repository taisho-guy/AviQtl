#include "window_manager/window_manager.hpp"
#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QSplashScreen>
#include <QTimer>
#include <QTranslator>
#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>

inline void initMyResources() { Q_INIT_RESOURCE(resources); }

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  initMyResources();

  app.setQuitOnLastWindowClosed(false);
  QApplication::setApplicationName(QStringLiteral("AviQtl"));

  app.setWindowIcon(QIcon(QStringLiteral(":/assets/icon.svg")));

  const QString appDir = QApplication::applicationDirPath();
  QString resourceDir =
      QDir(appDir + QStringLiteral("/../Resources")).canonicalPath();
  if (resourceDir.isEmpty()) {
    resourceDir = appDir;
  }

  QTranslator translator;
  if (translator.load(QLocale::system(), QStringLiteral("AviQtl"),
                      QStringLiteral("_"),
                      resourceDir + QStringLiteral("/i18n"))) {
    app.installTranslator(&translator);
  }

  QSplashScreen splash(
      QIcon(QStringLiteral(":/assets/splash.svg")).pixmap(128, 128));
  splash.show();

  QTimer::singleShot(500, [&]() {
    AviQtl::System::WindowManager::instance().spawnInitialWindows();
    splash.finish(AviQtl::System::WindowManager::instance().getMainWindow());
  });

  return app.exec();
}