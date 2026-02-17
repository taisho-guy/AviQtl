#include "effect_registry.hpp"
#include "rina_context.hpp"
#include "settings_manager.hpp"
#include "../../scripting/mod_engine.hpp"
#include "../../engine/timeline/ecs.hpp"
#include "timeline_controller.hpp"
#include "video_encoder.hpp"
#include "video_frame_provider.hpp"
#include "video_frame_store.hpp"
#include "window_manager.hpp"
#include <QApplication>
#include <QPixmap>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle> // 追加
#include <QQuickWindow>
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

    // 【追加】アプリケーションアイコンの設定
    // Qtのリソースシステム (:prefix) から読み込みます
    // SVGをアイコンに設定すると、ウィンドウマネージャ(KDE/Wayland)が適切にラスタライズしてくれます
    app.setWindowIcon(QIcon(":/assets/icon.svg"));

    // 設定のロード(インスタンス取得時にloadされる)
    Rina::Core::SettingsManager::instance().settings();

    // エフェクトレジストリの初期化
    Rina::Core::initializeStandardEffects();

    // --- Lua MODエンジンの初期化 (Phase 1 & 2) ---
    auto& modEngine = Rina::Scripting::ModEngine::instance();
    // ECSのインスタンス（g_ecsState）のアドレスをLuaに渡す
    void* ecsPtr = Rina::Engine::Timeline::ECS::instance().getInternalStatePtr();
    modEngine.initialize(ecsPtr);

    // プラグインのロード
    modEngine.loadPlugins();

    // Luaフック用のタイマー (約60FPSで同期)
    QTimer luaHookTimer;
    QObject::connect(&luaHookTimer, &QTimer::timeout, [&modEngine](){
        modEngine.onUpdate();
    });
    luaHookTimer.start(16);

    // 外部リソースのロード
    QString appDir = QCoreApplication::applicationDirPath();

    // 1. Filters (./effects)
    Rina::Core::EffectRegistry::instance().loadEffectsFromDirectory(appDir + "/effects");

    // 2. Objects (./objects)
    Rina::Core::EffectRegistry::instance().loadEffectsFromDirectory(appDir + "/objects");

    // 1. スタイルの強制適用 (KDE Plasma Native)
    // これにより、システムの色設定（ダークモード等）が自動的にQMLに反映される
    // 削除: Windows等で "org.kde.desktop" がない場合にクラッシュする原因となる
    // Qtデフォルトの自動判定に任せることで、OSごとのネイティブスタイル(WindowsVista等)が適用される
    QQuickStyle::setFallbackStyle("Fusion"); // Fallback for environments without a native style

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

    // ウィンドウ生成（バックグラウンドで生成）
    Rina::UI::WindowManager::instance().spawnInitialWindows(&engine);

    return app.exec();
}
