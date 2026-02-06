#include "effect_registry.hpp"
#include "rina_context.hpp"
#include "settings_manager.hpp"
#include "timeline_controller.hpp"
#include "video_encoder.hpp"
#include "video_frame_provider.hpp"
#include "video_frame_store.hpp"
#include "window_manager.hpp"
#include <QApplication>
#include <QPixmap>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle> // 追加
#include <QQuickWindow>
#include <QSplashScreen>
#include <QTimer>

int main(int argc, char *argv[]) {
    // --- マルチプラットフォーム対応 ハードウェアデコード設定 ---
    // 1. バックエンドをFFmpegに固定（Rinaのロジック依存のため）
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");

    // 2. 優先するHWデコーダリストを指定（環境依存のドライバ名は指定しない）
    // Linux(VAAPI/VDPAU/CUDA), Windows(D3D11VA/DXVA2), macOS(VideoToolbox) を網羅
    qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", "cuda,vaapi,vdpau,videotoolbox,d3d11va,dxva2");

    // 【追加】描画バックエンドをOpenGLに強制 (FBO/Texture共有のため必須)
    // これがないとLinux/AMDではVulkanが使われ、GPUパイプラインが壊れます
    qputenv("QSG_RHI_BACKEND", "opengl");

    // 1. アプリケーション初期化
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);
    app.setApplicationName("Rina");

    // 設定のロード(インスタンス取得時にloadされる)
    auto settings = Rina::Core::SettingsManager::instance().settings();
    int splashSize = settings.value("splashSize", 512).toInt();
    int splashDuration = settings.value("splashDuration", 1000).toInt();

    // --- スプラッシュスクリーンの表示 ---
    // SVGをQPixmapとして読み込み（高解像度対応のため大きめにレンダリング）
    QPixmap splashImg(":/assets/icon.svg");
    if (splashImg.isNull()) {
        qWarning() << "Failed to load splash image from :/assets/icon.svg";
        splashImg = QPixmap(splashSize, splashSize);
        splashImg.fill(Qt::black);
    } else {
        splashImg = splashImg.scaled(splashSize, splashSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // 最前面表示フラグを明示的に設定
    QSplashScreen splash(splashImg, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    splash.show();
    splash.raise(); // 明示的に最前面に
    splash.activateWindow();
    app.processEvents();

    // エフェクトレジストリの初期化
    splash.showMessage("エフェクトレジストリを初期化中...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    splash.raise();
    app.processEvents();
    Rina::Core::initializeStandardEffects();

    // 外部リソースのロード
    QString appDir = QCoreApplication::applicationDirPath();

    // 1. Filters (./effects)
    splash.showMessage("エフェクトを読み込み中...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    splash.raise();
    app.processEvents();
    Rina::Core::EffectRegistry::instance().loadEffectsFromDirectory(appDir + "/effects");

    // 2. Objects (./objects)
    splash.showMessage("オブジェクトを読み込み中...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    splash.raise();
    app.processEvents();
    Rina::Core::EffectRegistry::instance().loadEffectsFromDirectory(appDir + "/objects");

    // 1. スタイルの強制適用 (KDE Plasma Native)
    // これにより、システムの色設定（ダークモード等）が自動的にQMLに反映される
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle("org.kde.desktop");
    }
    QQuickStyle::setFallbackStyle("Fusion"); // 非KDE環境でも最低限見れるようにする

    splash.showMessage("UIを初期化中...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    splash.raise();
    app.processEvents();

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

    // ウィンドウ生成後もスプラッシュを最前面に保つ
    splash.raise();
    splash.activateWindow();
    app.processEvents();

    // スプラッシュを確実に閉じる（ウィンドウ生成完了後、少し遅延）
    QTimer::singleShot(splashDuration, [&splash]() { splash.close(); });

    return app.exec();
}
