#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "rina_context.hpp"
#include "window_manager.hpp"
#include "timeline_controller.hpp"

int main(int argc, char *argv[]) {
    // 1. アプリケーション初期化
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QGuiApplication app(argc, argv);
    app.setApplicationName("Rina");

    QQmlApplicationEngine engine;

    // TimelineBridge の登録
    // アプリケーション生存期間中維持されるインスタンスを作成
    auto* timelineController = new Rina::UI::TimelineController(&app);
    engine.rootContext()->setContextProperty("TimelineBridge", timelineController);

    // ウィンドウ生成
    Rina::UI::WindowManager::instance().spawnInitialWindows(&engine);

    return app.exec();
}
