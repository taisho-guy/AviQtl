#include "../../engine/plugin/audio_plugin_manager.hpp"
#include "../../engine/timeline/ecs.hpp"
#include "../../scripting/mod_engine.hpp"
#include "color_scheme_controller.hpp"
#include "effect_registry.hpp"
#include "rina_context.hpp"
#include "settings_manager.hpp"
#include "theme_controller.hpp"
#include "timeline_controller.hpp"
#include "video_encoder.hpp"
#include "video_frame_provider.hpp"
#include "video_frame_store.hpp"
#include "window_manager.hpp"
#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSplashScreen>
#include <QTimer>
#include <cstdio>
#include <cstring>
#include <lua.hpp>

extern "C" {
#include <libavutil/log.h>
}

static void rina_ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list vl) {
    char line[1024];
    va_list vl_copy;
    va_copy(vl_copy, vl);
    vsnprintf(line, sizeof(line), fmt, vl_copy);
    va_end(vl_copy);
    if (strstr(line, "Late SEI is not implemented")) {
        return;
    }
    av_log_default_callback(ptr, level, fmt, vl);
}

int main(int argc, char *argv[]) {
    qputenv("QSG_RHI_BACKEND", "vulkan");

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);
    app.setApplicationName("Rina");

    // メインスレッド上で設定管理を初期化する
    QVariantMap settings = Rina::Core::SettingsManager::instance().settings();
    Rina::Core::ThemeController::instance();

    av_log_set_callback(rina_ffmpeg_log_callback);
    app.setWindowIcon(QIcon(":/assets/icon.svg"));

    // スプラッシュ画面をヒープ領域に確保する
    int splashSize = settings.value("splashSize", 128).toInt();
    QPixmap splashPixmap = QIcon(":/assets/icon.svg").pixmap(splashSize, splashSize);
    auto *splash = new QSplashScreen(splashPixmap);
    splash->show();
    app.processEvents();

    QTimer luaHookTimer;
    auto &modEngine = Rina::Scripting::ModEngine::instance();
    QObject::connect(&luaHookTimer, &QTimer::timeout, [&modEngine]() { modEngine.onUpdate(); });
    luaHookTimer.start(Rina::Core::SettingsManager::instance().value("luaHookIntervalMs", 16).toInt());

    QQuickStyle::setFallbackStyle("Fusion");
    auto *colorSchemeController = new Rina::Core::ColorSchemeController(&app);
    QQmlApplicationEngine engine;

    auto *videoFrameStore = new Rina::Core::VideoFrameStore(&app);
    engine.addImageProvider("videoFrame", new Rina::Core::VideoFrameProvider(videoFrameStore));
    engine.rootContext()->setContextProperty("videoFrameStore", videoFrameStore);

    qmlRegisterType<Rina::Core::VideoEncoder>("Rina.Core", 1, 0, "VideoEncoder");
    engine.rootContext()->setContextProperty("SettingsManager", &Rina::Core::SettingsManager::instance());
    engine.rootContext()->setContextProperty("ColorSchemeController", colorSchemeController);

    auto *timelineController = new Rina::UI::TimelineController(&app);
    engine.rootContext()->setContextProperty("TimelineBridge", timelineController);
    timelineController->setVideoFrameStore(videoFrameStore);

    engine.rootContext()->setContextProperty("WindowManager", static_cast<QObject *>(&Rina::UI::WindowManager::instance()));

    // 遅延初期化処理を実行する
    QTimer::singleShot(10, [&engine, timelineController, &modEngine, splash]() {
        Rina::Core::initializeStandardEffects();
        modEngine.initialize(nullptr);
        modEngine.registerController(timelineController);
        modEngine.loadPlugins();

        QString appDir = QCoreApplication::applicationDirPath();
        Rina::Core::EffectRegistry::instance().loadEffectsFromDirectory(appDir + "/effects");
        Rina::Core::EffectRegistry::instance().loadEffectsFromDirectory(appDir + "/objects");

        // 音声プラグインの読み込み完了時にスプラッシュ画面を消去する
        QObject::connect(&Rina::Engine::Plugin::AudioPluginManager::instance(), &Rina::Engine::Plugin::AudioPluginManager::pluginsReady, splash, [&engine, splash](int) {
            Rina::UI::WindowManager::instance().spawnInitialWindows(&engine);
            splash->finish(nullptr);
            splash->deleteLater();
        });

        // 音声プラグインの初期化とスキャンを開始する
        Rina::Engine::Plugin::AudioPluginManager::instance().initialize();
    });

    return app.exec();
}
