#include "components/object_kind_component.hpp"
#include "components/scene_component.hpp"
#include "components/time_range_component.hpp"
#include "components/transform_component.hpp"
#include "ecs/world.hpp"
#include "systems/timeline_evaluation_system.hpp"
#include "windowing/window_coordinator.hpp"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QIcon>
#include <QSplashScreen>
#include <QTimer>
#include <QTranslator>
#include <SDL3/SDL.h>

inline void initMyResources() { Q_INIT_RESOURCE(resources); }

void verifyEcs() {
  using namespace AviQtl::Engine;
  World world;

  // 1. Create Scene Entity
  EntityId sceneEntity = world.createEntity();
  SceneComponent scene;
  scene.width = 1920;
  scene.height = 1080;
  scene.fps = 60.0;
  scene.totalFrames = 3600;
  world.addComponent(sceneEntity, std::move(scene));
  qDebug() << "[ECS Test] Created Scene Entity:" << sceneEntity;

  // 2. Create Clip 1 (Video: frames 0 to 100)
  EntityId clip1 = world.createEntity();
  world.addComponent(clip1, TimeRangeComponent{0, 100, 1});
  world.addComponent(clip1,
                     TransformComponent{0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f});
  world.addComponent(clip1,
                     ObjectKindComponent{ObjectKind::Video, "Intro Video"});
  qDebug() << "[ECS Test] Created Clip 1 Entity:" << clip1;

  // 3. Create Clip 2 (Image overlay: frames 50 to 150)
  EntityId clip2 = world.createEntity();
  world.addComponent(clip2, TimeRangeComponent{50, 100, 2});
  world.addComponent(
      clip2, TransformComponent{100.0f, 100.0f, 0.5f, 0.5f, 45.0f, 0.8f});
  world.addComponent(clip2,
                     ObjectKindComponent{ObjectKind::Image, "Logo Image"});
  qDebug() << "[ECS Test] Created Clip 2 Entity:" << clip2;

  // 4. Evaluate at frame 30
  qDebug() << "[ECS Test] Evaluating active clips at frame 30:";
  auto activeClips30 =
      TimelineEvaluationSystem::getActiveClipsAtFrame(world, 30);
  for (EntityId e : activeClips30) {
    auto *kind = world.getComponent<ObjectKindComponent>(e);
    auto *range = world.getComponent<TimeRangeComponent>(e);
    qDebug() << "  - Entity:" << e << (kind ? kind->name.c_str() : "unknown")
             << "Layer:" << (range ? range->layer : 0);
  }

  // 5. Evaluate at frame 75
  qDebug() << "[ECS Test] Evaluating active clips at frame 75:";
  auto activeClips75 =
      TimelineEvaluationSystem::getActiveClipsAtFrame(world, 75);
  for (EntityId e : activeClips75) {
    auto *kind = world.getComponent<ObjectKindComponent>(e);
    auto *range = world.getComponent<TimeRangeComponent>(e);
    qDebug() << "  - Entity:" << e << (kind ? kind->name.c_str() : "unknown")
             << "Layer:" << (range ? range->layer : 0);
  }
}

int main(int argc, char *argv[]) {
  verifyEcs();

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
