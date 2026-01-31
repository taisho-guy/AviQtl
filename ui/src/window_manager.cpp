#include "window_manager.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml>

namespace Rina::UI {

// コンストラクタ定義
WindowManager::WindowManager(QObject *parent) : QObject(parent) {}

// 実装: シングルトンインスタンス
WindowManager &WindowManager::instance() {
    static WindowManager inst(nullptr);
    return inst;
}

void WindowManager::spawnInitialWindows(QQmlEngine *engine) {
    m_engine = engine;

    // プロジェクトランチャーのみを表示
    QQmlComponent component(engine, QUrl("qrc:/qt/qml/Rina/ui/qml/ProjectLauncherWindow.qml"));
    QObject *obj = component.create();

    if (component.status() != QQmlComponent::Ready) {
        qWarning() << "Failed to create ProjectLauncherWindow:" << component.errorString();
        // フォールバック: 従来通りすぐに編集画面を開く
        spawnWindow(engine, "main", "qrc:/qt/qml/Rina/ui/qml/MainWindow.qml", "Rina Main Preview", 640, 480, 100, 100, true);
        spawnWindow(engine, "timeline", "qrc:/qt/qml/Rina/ui/qml/TimelineWindow.qml", "Timeline", 1280, 300, 100, 600, true);
        spawnWindow(engine, "objectSettings", "qrc:/qt/qml/Rina/ui/qml/SettingDialog.qml", "Object Settings", 400, 600, 800, 420, false);
        return;
    }

    QQuickWindow *launcher = qobject_cast<QQuickWindow *>(obj);
    if (launcher) {
        // プロジェクトが選択されたら、メインウィンドウを開く
        QObject::connect(launcher, SIGNAL(projectSelected(QString, int, int, double, int)), this, SLOT(onProjectSelected(QString, int, int, double, int)));
        registerWindow("launcher", launcher);
        launcher->show();
    } else {
        qWarning() << "ProjectLauncherWindow is not a QQuickWindow";
    }
}

void WindowManager::onProjectSelected(const QString &path, int w, int h, double fps, int frames) {
    if (!m_engine) {
        qWarning() << "Engine not available in onProjectSelected";
        return;
    }

    // ランチャーを閉じる
    QPointer<QQuickWindow> launcher = m_windows.value("launcher");
    if (launcher) {
        launcher->close();
    }

    // プロジェクト設定を反映してメインウィンドウを開く
    spawnWindow(m_engine, "main", "qrc:/qt/qml/Rina/ui/qml/MainWindow.qml", "Rina Main Preview", 640, 480, 100, 100, true);
    spawnWindow(m_engine, "timeline", "qrc:/qt/qml/Rina/ui/qml/TimelineWindow.qml", "Timeline", 1280, 300, 100, 600, true);
    spawnWindow(m_engine, "projectSettings", "qrc:/qt/qml/Rina/ui/qml/ProjectSettingsWindow.qml", "Project Settings", 450, 250, 800, 100, false);
    spawnWindow(m_engine, "objectSettings", "qrc:/qt/qml/Rina/ui/qml/SettingDialog.qml", "Object Settings", 400, 600, 800, 420, false);
    spawnWindow(m_engine, "systemSettings", "qrc:/qt/qml/Rina/ui/qml/SystemSettingsWindow.qml", "System Settings", 600, 500, 200, 200, false);

    // 設定の反映
    QObject *bridge = m_engine->rootContext()->contextProperty("TimelineBridge").value<QObject *>();
    if (bridge) {
        if (!path.isEmpty()) {
            QMetaObject::invokeMethod(bridge, "loadProject", Q_ARG(QString, path));
        } else {
            QObject *project = bridge->property("project").value<QObject *>();
            if (project) {
                project->setProperty("width", w);
                project->setProperty("height", h);
                project->setProperty("fps", fps);
                project->setProperty("totalFrames", frames);
            }
        }
    }
}

void WindowManager::spawnWindow(QQmlEngine *engine, const QString &id, const QString &urlStr, const QString &title, int w, int h, int x, int y, bool visible) {
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
    if (auto win = qobject_cast<QQuickWindow *>(obj)) {
        win->setTitle(title);
        win->resize(w, h);
        win->setX(x);
        win->setY(y);
        registerWindow(id, win);
        if (visible)
            win->show();
        else
            win->hide();
    } else {
        // QQuickWindowではなかった場合 (View3D直置きなど)、ウィンドウを作る
        auto window = new QQuickWindow();
        window->setTitle(title);
        window->resize(w, h);
        window->setX(x);
        window->setY(y);

        auto item = qobject_cast<QQuickItem *>(obj);
        if (item) {
            item->setParentItem(window->contentItem());
        }
        registerWindow(id, window);
        if (visible)
            window->show();
        else
            window->hide();
    }
}

void WindowManager::registerWindow(const QString &id, QQuickWindow *win) {
    m_windows[id] = win;

    // ユーザーが×で閉じたり、hide/showした場合の同期
    connect(win, &QQuickWindow::visibleChanged, this, [this, id]() { emitVisibilityChanged(id); });
    connect(win, &QObject::destroyed, this, [this, id]() {
        m_windows.remove(id);
        emitVisibilityChanged(id);
    });

    // メインが閉じられたら「全終了」
    if (id == "main") {
        connect(win, &QQuickWindow::closing, this, [this](QQuickCloseEvent *e) {
            Q_UNUSED(e);
            requestQuit();
        });
    }

    emitVisibilityChanged(id);
}

void WindowManager::emitVisibilityChanged(const QString &id) {
    if (id == "timeline")
        emit timelineVisibleChanged();
    if (id == "projectSettings")
        emit projectSettingsVisibleChanged();
    if (id == "objectSettings")
        emit objectSettingsVisibleChanged();
    if (id == "systemSettings")
        emit systemSettingsVisibleChanged();
}

bool WindowManager::isVisible(const QString &id) const {
    QPointer<QQuickWindow> w = m_windows.value(id);
    return w ? w->isVisible() : false;
}
void WindowManager::setVisible(const QString &id, bool visible) {
    QPointer<QQuickWindow> w = m_windows.value(id);
    if (!w)
        return;
    if (visible)
        w->show();
    else
        w->hide();
    if (visible) {
        w->raise();
        w->requestActivate();
    }
}
void WindowManager::toggleVisible(const QString &id) { setVisible(id, !isVisible(id)); }
void WindowManager::raiseWindow(const QString &id) {
    QPointer<QQuickWindow> w = m_windows.value(id);
    if (!w)
        return;
    w->show();
    w->raise();
    w->requestActivate();
}

void WindowManager::requestQuit() {
    // 全Windowを閉じる（main自身も含めてOK）
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (it.value())
            it.value()->close();
    }
    QCoreApplication::quit();
}

bool WindowManager::timelineVisible() const { return isVisible("timeline"); }
void WindowManager::setTimelineVisible(bool v) { setVisible("timeline", v); }
bool WindowManager::projectSettingsVisible() const { return isVisible("projectSettings"); }
void WindowManager::setProjectSettingsVisible(bool v) { setVisible("projectSettings", v); }
bool WindowManager::objectSettingsVisible() const { return isVisible("objectSettings"); }
void WindowManager::setObjectSettingsVisible(bool v) { setVisible("objectSettings", v); }
bool WindowManager::systemSettingsVisible() const { return isVisible("systemSettings"); }
void WindowManager::setSystemSettingsVisible(bool v) { setVisible("systemSettings", v); }
} // namespace Rina::UI
