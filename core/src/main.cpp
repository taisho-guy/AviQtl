#include "../../engine/plugin/audio_plugin_manager.hpp"
#include "../../engine/timeline/ecs.hpp"
#include "../../scripting/mod_engine.hpp"
#include "effect_registry.hpp"
#include "rina_context.hpp"
#include "settings_manager.hpp"
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
#include <QQuickStyle> // 追加
#include <QQuickWindow>
#include <QSplashScreen>
#include <QTimer>
#include <lua.hpp>

int main(int argc, char *argv[]) {
    // --- マルチプラットフォーム対応 ハードウェアデコード設定 ---
    // 1. バックエンドをFFmpegに固定（Rinaのロジック依存のため）
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");

    // 2. 優先するHWデコーダリストを指定（環境依存のドライバ名は指定しない）
    // Linux(VAAPI/VDPAU/CUDA), Windows(D3D11VA/DXVA2), macOS(VideoToolbox) を網羅
    qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", "cuda,vaapi,vdpau,videotoolbox,d3d11va,dxva2");

    // 1. アプリケーション初期化
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);
    app.setApplicationName("Rina");

    // アプリケーションアイコンの設定
    app.setWindowIcon(QIcon(":/assets/icon.svg"));

    // 設定のロード(インスタンス取得時にloadされる)
    QVariantMap settings = Rina::Core::SettingsManager::instance().settings();

    // スプラッシュスクリーンの表示
    int splashSize = settings.value("splashSize", 512).toInt();
    QPixmap splashPixmap = QIcon(":/assets/splash.svg").pixmap(splashSize, splashSize);
    QSplashScreen splash(splashPixmap);
    splash.show();
    app.processEvents();

    // Luaフック用のタイマー
    QTimer luaHookTimer;
    auto &modEngine = Rina::Scripting::ModEngine::instance();
    QObject::connect(&luaHookTimer, &QTimer::timeout, [&modEngine]() { modEngine.onUpdate(); });
    luaHookTimer.start(16);

    QQuickStyle::setFallbackStyle("Fusion");

    QQmlApplicationEngine engine;

    // VideoFrameProviderの登録
    auto *videoFrameStore = new Rina::Core::VideoFrameStore(&app);
    engine.addImageProvider("videoFrame", new Rina::Core::VideoFrameProvider(videoFrameStore));
    engine.rootContext()->setContextProperty("videoFrameStore", videoFrameStore);

    // VideoEncoderをQMLでインスタンス化可能な型として登録
    qmlRegisterType<Rina::Core::VideoEncoder>("Rina.Core", 1, 0, "VideoEncoder");

    // SettingsManager の登録
    engine.rootContext()->setContextProperty("SettingsManager", &Rina::Core::SettingsManager::instance());

    // TimelineBridge の登録
    // アプリケーション生存期間中維持されるインスタンスを作成
    auto *timelineController = new Rina::UI::TimelineController(&app);
    engine.rootContext()->setContextProperty("TimelineBridge", timelineController);

    // TimelineControllerにVideoFrameStoreを渡す
    timelineController->setVideoFrameStore(videoFrameStore);

    // WindowManager をQMLから触れるように公開
    engine.rootContext()->setContextProperty("WindowManager", static_cast<QObject *>(&Rina::UI::WindowManager::instance()));

    // 初期化処理をイベントループ開始後に遅延させることで、スプラッシュスクリーンを確実に描画する
    QTimer::singleShot(10, [&] {
        // エフェクトレジストリの初期化
        Rina::Core::initializeStandardEffects();

        // オーディオプラグインの初期化
        Rina::Engine::Plugin::AudioPluginManager::instance().scanPlugins();

        // Luaエンジンの初期化
        // ECSのインスタンス（g_ecsState）のアドレスをLuaに渡す
        void *ecsPtr = Rina::Engine::Timeline::ECS::instance().getInternalStatePtr();
        modEngine.initialize(ecsPtr);
        // Lua API に controller を登録
        modEngine.registerController(timelineController);
        // プラグインのロード
        modEngine.loadPlugins();

        // 外部リソースのロード
        QString appDir = QCoreApplication::applicationDirPath();
        Rina::Core::EffectRegistry::instance().loadEffectsFromDirectory(appDir + "/effects");
        Rina::Core::EffectRegistry::instance().loadEffectsFromDirectory(appDir + "/objects");

        // ウィンドウ生成（バックグラウンドで生成）
        Rina::UI::WindowManager::instance().spawnInitialWindows(&engine);

        // スプラッシュを閉じる
        splash.finish(nullptr);
    });

    return app.exec();
}
