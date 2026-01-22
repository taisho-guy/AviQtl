#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include "rina_context.hpp"
#include <QQuickStyle> // 追加
#include "window_manager.hpp"
#include "timeline_controller.hpp"
#include "effect_registry.hpp"

int main(int argc, char *argv[]) {
    // 1. アプリケーション初期化
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);
    app.setApplicationName("Rina");

    // エフェクトレジストリの初期化
    Rina::Core::initializeStandardEffects();

    // 1. スタイルの強制適用 (KDE Plasma Native)
    // これにより、システムの色設定（ダークモード等）が自動的にQMLに反映される
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle("org.kde.desktop");
    }
    QQuickStyle::setFallbackStyle("Fusion"); // 非KDE環境でも最低限見れるようにする

    QQmlApplicationEngine engine;

    // TimelineBridge の登録
    // アプリケーション生存期間中維持されるインスタンスを作成
    auto* timelineController = new Rina::UI::TimelineController(&app);
    engine.rootContext()->setContextProperty("TimelineBridge", timelineController);

    // WindowManager をQMLから触れるように公開
    engine.rootContext()->setContextProperty("WindowManager", static_cast<QObject*>(&Rina::UI::WindowManager::instance()));

    // ウィンドウ生成
    Rina::UI::WindowManager::instance().spawnInitialWindows(&engine);

    return app.exec();
}
