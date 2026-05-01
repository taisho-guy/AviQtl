#include "../../engine/plugin/audio_plugin_manager.hpp"
#include "../../engine/timeline/ecs.hpp"
#include "../../scripting/mod_engine.hpp"
#include "aviqtl_context.hpp"
#include "compute_effect.hpp"
#include "effect_registry.hpp"
#include "package_manager.hpp"
#include "settings_manager.hpp"
#include "theme_controller.hpp"
#include "timeline_controller.hpp"
#include "version.hpp"
#include "video_encoder.hpp"
#include "video_frame_provider.hpp"
#include "video_frame_store.hpp"
#include "window_manager.hpp"
#include "workspace.hpp"
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

static void aviqtl_ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list vl) {
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
    QApplication::setApplicationName(QStringLiteral("AviQtl"));

    QString appDir = QCoreApplication::applicationDirPath();

    // システムのロケールに合わせて翻訳ファイルをロード
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = QStringLiteral("AviQtl_") + QLocale(locale).name();
        if (translator.load(baseName, appDir + QStringLiteral("/i18n"))) {
            qDebug() << QStringLiteral("[Main] 翻訳ファイルをロードしました:") << baseName;
            QApplication::installTranslator(&translator);
            break;
        }
    }

    // メインスレッド上で設定管理を初期化する
    QVariantMap settings = AviQtl::Core::SettingsManager::instance().settings();
    AviQtl::Core::ThemeController::instance();

    av_log_set_callback(aviqtl_ffmpeg_log_callback);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/icon.svg")));

    // スプラッシュ画面をヒープ領域に確保する
    int splashSize = settings.value(QStringLiteral("splashSize"), 128).toInt();
    QPixmap splashPixmap = QIcon(QStringLiteral(":/assets/splash.svg")).pixmap(splashSize, splashSize);
    auto *splash = new QSplashScreen(splashPixmap);
    splash->show();
    QApplication::processEvents();

    QTimer luaHookTimer;
    auto &modEngine = AviQtl::Scripting::ModEngine::instance();
    QObject::connect(&luaHookTimer, &QTimer::timeout, [&modEngine]() -> void { modEngine.onUpdate(); });
    luaHookTimer.start(AviQtl::Core::SettingsManager::instance().value(QStringLiteral("luaHookIntervalMs"), 16).toInt());

    QQuickStyle::setFallbackStyle(QStringLiteral("Fusion"));
    QQmlApplicationEngine engine;

    auto *videoFrameStore = new AviQtl::Core::VideoFrameStore(&app);
    engine.addImageProvider(QStringLiteral("videoFrame"), new AviQtl::Core::VideoFrameProvider(videoFrameStore));
    engine.rootContext()->setContextProperty(QStringLiteral("videoFrameStore"), videoFrameStore);

    qmlRegisterType<AviQtl::Core::VideoEncoder>("AviQtl.Core", 1, 0, "VideoEncoder");
    qmlRegisterType<AviQtl::UI::Effects::ComputeEffect>("AviQtl.Compute", 1, 0, "ComputeEffect");
    engine.rootContext()->setContextProperty(QStringLiteral("SettingsManager"), &AviQtl::Core::SettingsManager::instance());
    qmlRegisterUncreatableType<AviQtl::UI::TimelineController>("AviQtl.UI", 1, 0, "TimelineController", "Managed by C++");

    engine.rootContext()->setContextProperty(QStringLiteral("AviQtlVersion"), QString::fromUtf8(AviQtl::VERSION_STRING));
    engine.rootContext()->setContextProperty(QStringLiteral("AviQtlVersionCodename"), QString::fromUtf8(AviQtl::VERSION_CODENAME));
    engine.rootContext()->setContextProperty(QStringLiteral("PackageManager"), &AviQtl::Core::PackageManager::instance());
    auto *workspace = new AviQtl::UI::Workspace(&app);
    engine.rootContext()->setContextProperty(QStringLiteral("Workspace"), workspace);
    QObject::connect(workspace, &AviQtl::UI::Workspace::currentTimelineChanged, &app, [&modEngine, workspace]() {
        if (workspace->currentTimeline()) {
            modEngine.registerController(workspace->currentTimeline());
        }
    });
    workspace->setVideoFrameStore(videoFrameStore);

    engine.rootContext()->setContextProperty(QStringLiteral("WindowManager"), static_cast<QObject *>(&AviQtl::UI::WindowManager::instance()));

    // 遅延初期化処理を実行する
    QTimer::singleShot(10, &engine, [&engine, workspace, &modEngine, splash]() -> void {
        AviQtl::Core::initializeStandardEffects();
        modEngine.initialize(nullptr);
        modEngine.registerController(workspace->currentTimeline());
        modEngine.loadPlugins();

        QString appDir = QCoreApplication::applicationDirPath();
        AviQtl::Core::EffectRegistry::instance().loadEffectsFromDirectory(appDir + QStringLiteral("/effects"));
        AviQtl::Core::EffectRegistry::instance().loadEffectsFromDirectory(appDir + QStringLiteral("/objects"));

        // 音声プラグインの読み込み完了時にスプラッシュ画面を消去する
        QObject::connect(&AviQtl::Engine::Plugin::AudioPluginManager::instance(), &AviQtl::Engine::Plugin::AudioPluginManager::pluginsReady, splash, [&engine, splash](int) -> void {
            AviQtl::UI::WindowManager::instance().spawnInitialWindows(&engine);
            splash->finish(nullptr);
            splash->deleteLater();
        });

        // 音声プラグインの初期化とスキャンを開始する
        AviQtl::Engine::Plugin::AudioPluginManager::instance().initialize();
    });

    return QApplication::exec();
}
