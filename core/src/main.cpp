#include "effect_registry.hpp"
#include "rina_context.hpp"
#include "settings_manager.hpp"
#include "video_frame_provider.hpp"
#include "video_frame_store.hpp"
#include "timeline_controller.hpp"
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
    int startupDelay = settings.value("appStartupDelay", 1000).toInt();

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

    // SettingsManager の登録
    engine.rootContext()->setContextProperty("SettingsManager", &Rina::Core::SettingsManager::instance());

    // TimelineBridge の登録
    // アプリケーション生存期間中維持されるインスタンスを作成
    auto *timelineController = new Rina::UI::TimelineController(&app);
    engine.rootContext()->setContextProperty("TimelineBridge", timelineController);

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

    // CompositeViewをTimelineControllerに登録（GPU最適化のため）
    QTimer::singleShot(startupDelay, [&]() {
        QObject *bridge = engine.rootContext()->contextProperty("TimelineBridge").value<QObject *>();
        auto windows = engine.rootObjects();

        for (auto *obj : windows) {
            if (QQuickWindow *win = qobject_cast<QQuickWindow *>(obj)) {
                QMetaObject::invokeMethod(bridge, "setCompositeView", Q_ARG(QQuickWindow *, win));
                break;
            }
        }
    });

    return app.exec();
}
