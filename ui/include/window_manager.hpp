#pragma once
#include <QQmlEngine>
#include <QQuickWindow>
#include <QList>

namespace Rina::UI {
    class WindowManager {
    public:
        static WindowManager& instance();
        void spawnInitialWindows(QQmlEngine* engine);
        void spawnWindow(QQmlEngine* engine, const QString& urlStr, const QString& title, int w, int h, int x, int y);
        
    private:
        WindowManager() = default;
        QList<QQuickWindow*> m_windows;
    };
}