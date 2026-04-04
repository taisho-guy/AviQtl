#include "../../engine/plugin/audio_plugin_manager.hpp"
#include "../../engine/timeline/ecs.hpp"
#include "../../scripting/mod_engine.hpp"
#include "color_scheme_controller.hpp"
#include "effect_registry.hpp"
#include "rina_context.hpp"
#include "settings_manager.hpp"
#include "theme_controller.hpp"
#include "timeline_controller.hpp"
#include "update_checker.hpp"
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
#include <QTranslator>
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
    if (strstr(line, "Late SEI is not implemented") != nullptr) {
        return;
    }
    av_log_default_callback(ptr, level, fmt, vl);
}

auto main(int argc, char *argv[]) -> int {
    qputenv("QSG_RHI_BACKEND", "vulkan");

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Rina"));

    QString appDir = QCoreApplication::applicationDirPath();

    // システムのロケールに合わせて翻訳ファイルをロード
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = QStringLiteral("Rina_") + QLocale(locale).name();
        if (translator.load(baseName, appDir + QStringLiteral("/i18n"))) {
            qDebug() << QStringLiteral("[Main] 翻訳ファイルをロードしました:") << baseName;
            QApplication::installTranslator(&translator);
            break;
        }
    }

    // メインスレッド上で設定管理を初期化する
    QVariantMap settings = Rina::Core::SettingsManager::instance().settings();
    Rina::Core::ThemeController::instance();

    av_log_set_callback(rina_ffmpeg_log_callback);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/icon.svg")));

    // スプラッシュ画面をヒープ領域に確保する
    int splashSize = settings.value(QStringLiteral("splashSize"), 128).toInt();
    QPixmap splashPixmap = QIcon(QStringLiteral(":/assets/splash.svg")).pixmap(splashSize, splashSize);
    auto *splash = new QSplashScreen(splashPixmap);
    splash->show();
    QApplication::processEvents();

    QTimer luaHookTimer;
    auto &modEngine = Rina::Scripting::ModEngine::instance();
    QObject::connect(&luaHookTimer, &QTimer::timeout, [&modEngine]() -> void { modEngine.onUpdate(); });
    luaHookTimer.start(Rina::Core::SettingsManager::instance().value(QStringLiteral("luaHookIntervalMs"), 16).toInt());

    QQuickStyle::setFallbackStyle(QStringLiteral("Fusion"));
    auto *colorSchemeController = new Rina::Core::ColorSchemeController(&app);
    QQmlApplicationEngine engine;

    auto *videoFrameStore = new Rina::Core::VideoFrameStore(&app);
    engine.addImageProvider(QStringLiteral("videoFrame"), new Rina::Core::VideoFrameProvider(videoFrameStore));
    engine.rootContext()->setContextProperty(QStringLiteral("videoFrameStore"), videoFrameStore);

    qmlRegisterType<Rina::Core::VideoEncoder>("Rina.Core", 1, 0, "VideoEncoder");
    engine.rootContext()->setContextProperty(QStringLiteral("SettingsManager"), &Rina::Core::SettingsManager::instance());
    engine.rootContext()->setContextProperty(QStringLiteral("ColorSchemeController"), colorSchemeController);

    auto *timelineController = new Rina::UI::TimelineController(&app);
    engine.rootContext()->setContextProperty(QStringLiteral("TimelineBridge"), timelineController);
    timelineController->setVideoFrameStore(videoFrameStore);

    engine.rootContext()->setContextProperty(QStringLiteral("WindowManager"), static_cast<QObject *>(&Rina::UI::WindowManager::instance()));

    auto *updateChecker = new Rina::Core::UpdateChecker(&app);
    engine.rootContext()->setContextProperty(QStringLiteral("UpdateChecker"), updateChecker);

    // 遅延初期化処理を実行する
    QTimer::singleShot(10, &engine, [&engine, timelineController, &modEngine, splash, updateChecker]() -> void {
        Rina::Core::initializeStandardEffects();
        modEngine.initialize(nullptr);
        modEngine.registerController(timelineController);
        modEngine.loadPlugins();

        QString appDir = QCoreApplication::applicationDirPath();
        Rina::Core::EffectRegistry::instance().loadEffectsFromDirectory(appDir + QStringLiteral("/effects"));
        Rina::Core::EffectRegistry::instance().loadEffectsFromDirectory(appDir + QStringLiteral("/objects"));

        // 音声プラグインの読み込み完了時にスプラッシュ画面を消去する
        QObject::connect(&Rina::Engine::Plugin::AudioPluginManager::instance(), &Rina::Engine::Plugin::AudioPluginManager::pluginsReady, splash, [&engine, splash, updateChecker](int) -> void {
            Rina::UI::WindowManager::instance().spawnInitialWindows(&engine);
            updateChecker->checkOnStartup();
            splash->finish(nullptr);
            splash->deleteLater();
        });

        // 音声プラグインの初期化とスキャンを開始する
        Rina::Engine::Plugin::AudioPluginManager::instance().initialize();
    });

    return QApplication::exec();
}
