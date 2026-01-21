#include "window_manager.hpp"
#include <QQuickWindow>
#include <QQmlComponent>
#include <QQuickItem>
#include <QList>
#include <QDebug>

namespace Rina::UI {

    // 実装: シングルトンインスタンス
    WindowManager& WindowManager::instance() {
        static WindowManager inst;
        return inst;
    }

    void WindowManager::spawnInitialWindows(QQmlEngine* engine) {
        // メインビュー: Windowルート(MainWindow)を開く（CompositeViewはItemルートのため）
        spawnWindow(engine, "qrc:/qml/MainWindow.qml", "Rina Main Preview", 640, 480, 100, 100);
        
        // タイムライン
        spawnWindow(engine, "qrc:/qml/TimelineWindow.qml", "Timeline", 1280, 300, 100, 600);

        // プロジェクト設定
        spawnWindow(engine, "qrc:/qml/SettingsWindow.qml", "Project Settings", 400, 300, 800, 100);
    }

    void WindowManager::spawnWindow(QQmlEngine* engine, const QString& urlStr, const QString& title, int w, int h, int x, int y) {
        if (!engine) {
            qWarning() << "WindowManager: Engine is null!";
            return;
        }

        QQmlComponent comp(engine, QUrl(urlStr));
        if (comp.status() != QQmlComponent::Ready) {
            qWarning() << "QML Error (" << title << "):" << comp.errorString();
            return;
        }
        
        QObject *obj = comp.create();
        if (auto win = qobject_cast<QQuickWindow*>(obj)) {
             win->setTitle(title);
             win->resize(w, h);
             win->show();
             m_windows.append(win);
        } else {
            // QQuickWindowではなかった場合 (View3D直置きなど)、ウィンドウを作る
            auto window = new QQuickWindow();
            window->setTitle(title);
            window->resize(w, h);
            
            auto item = qobject_cast<QQuickItem*>(obj);
            if (item) {
                item->setParentItem(window->contentItem());
            }
            window->show();
            m_windows.append(window);
        }
    }
}
