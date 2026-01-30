#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSplashScreen>
#include <QPixmap>
#include <QTimer>
#include "rina_context.hpp"
#include <QQuickStyle> // 追加
#include "window_manager.hpp"
#include "timeline_controller.hpp"
#include "effect_registry.hpp"
#include "settings_manager.hpp"

int main(int argc, char *argv[]) {
    // 1. アプリケーション初期化
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);
    app.setApplicationName("Rina");

    // --- スプラッシュスクリーンの表示 ---
    // SVGをQPixmapとして読み込み（高解像度対応のため大きめにレンダリング）
    QPixmap splashImg(":/assets/icon.svg");
    if (splashImg.isNull()) {
        qWarning() << "Failed to load splash image from :/assets/icon.svg";
        splashImg = QPixmap(512, 512);
        splashImg.fill(Qt::black);
    } else {
        splashImg = splashImg.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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

    // SettingsManager の登録
    engine.rootContext()->setContextProperty("SettingsManager", &Rina::Core::SettingsManager::instance());

    // TimelineBridge の登録
    // アプリケーション生存期間中維持されるインスタンスを作成
    auto* timelineController = new Rina::UI::TimelineController(&app);
    engine.rootContext()->setContextProperty("TimelineBridge", timelineController);

    // WindowManager をQMLから触れるように公開
    engine.rootContext()->setContextProperty("WindowManager", static_cast<QObject*>(&Rina::UI::WindowManager::instance()));

    // ウィンドウ生成（バックグラウンドで生成）
    Rina::UI::WindowManager::instance().spawnInitialWindows(&engine);

    // ウィンドウ生成後もスプラッシュを最前面に保つ
    splash.raise();
    splash.activateWindow();
    app.processEvents();

    // スプラッシュを確実に閉じる（ウィンドウ生成完了後、少し遅延）
    QTimer::singleShot(800, [&splash]() {
        splash.close();
    });

    return app.exec();
}
