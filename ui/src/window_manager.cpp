#include "window_manager.hpp"
#include "update_checker.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml>

namespace Rina::UI {

WindowManager::WindowManager(QObject *parent) : QObject(parent) {}

auto WindowManager::instance() -> WindowManager & {
    static WindowManager inst(nullptr);
    return inst;
}

void WindowManager::spawnInitialWindows(QQmlEngine *engine) {
    m_engine = engine;

    QQmlComponent component(engine, QUrl(QStringLiteral("qrc:/qt/qml/Rina/ui/qml/ProjectLauncherWindow.qml")));
    QObject *obj = component.create();

    auto *launcher = qobject_cast<QQuickWindow *>(obj);
    if (launcher != nullptr) {
        // プロジェクトが選択されたらメインウィンドウを開く
        QObject::connect(launcher, SIGNAL(projectSelected(QString, int, int, double)), this, SLOT(onProjectSelected(QString, int, int, double)));
        registerWindow(QStringLiteral("launcher"), launcher);
        launcher->show();
    } else {
        qWarning() << "ProjectLauncherWindowはQQuickWindowではありません";
    }
}

void WindowManager::onProjectSelected(const QString &path, int w, int h, double fps) { // NOLINT(bugprone-easily-swappable-parameters)
    if (!m_engine) {
        qWarning() << "onProjectSelectedでQMLエンジンが利用できません";
        return;
    }

    // ランチャーを閉じる
    QPointer<QQuickWindow> launcher = m_windows.value(QStringLiteral("launcher"));
    if (launcher) {
        launcher->close();
    }

    // プロジェクト設定を反映してメインウィンドウを開く
    spawnWindow(m_engine, QStringLiteral("main"), QStringLiteral("qrc:/qt/qml/Rina/ui/qml/MainWindow.qml"), tr("Rina メインプレビュー"), 640, 480, 100, 100, true);
    spawnWindow(m_engine, QStringLiteral("timeline"), QStringLiteral("qrc:/qt/qml/Rina/ui/qml/TimelineWindow.qml"), tr("タイムライン"), 1280, 300, 100, 600, true);
    spawnWindow(m_engine, QStringLiteral("projectSettings"), QStringLiteral("qrc:/qt/qml/Rina/ui/qml/ProjectSettingsWindow.qml"), tr("プロジェクト設定"), 450, 250, 800, 100, false);
    spawnWindow(m_engine, QStringLiteral("objectSettings"), QStringLiteral("qrc:/qt/qml/Rina/ui/qml/SettingDialog.qml"), tr("オブジェクト設定"), 400, 600, 800, 420, false);
    spawnWindow(m_engine, QStringLiteral("systemSettings"), QStringLiteral("qrc:/qt/qml/Rina/ui/qml/SystemSettingsWindow.qml"), tr("システム設定"), 600, 500, 200, 200, false);
    spawnWindow(m_engine, QStringLiteral("about"), QStringLiteral("qrc:/qt/qml/Rina/ui/qml/AboutWindow.qml"), tr("Rinaについて"), 400, 250, 400, 300, false);
    spawnWindow(m_engine, QStringLiteral("sceneSettings"), QStringLiteral("qrc:/qt/qml/Rina/ui/qml/SceneSettingsWindow.qml"), tr("シーン設定"), 450, 300, 300, 200, false);
    spawnWindow(m_engine, QStringLiteral("export"), QStringLiteral("qrc:/qt/qml/Rina/ui/qml/ExportDialog.qml"), tr("メディアの書き出し"), 620, 580, 240, 160, false);
    spawnWindow(m_engine, QStringLiteral("easingConfig"), QStringLiteral("qrc:/qt/qml/Rina/ui/qml/common/EasingConfigWindow.qml"), tr("補間設定"), 820, 540, 420, 180, false);

    spawnWindow(m_engine, QStringLiteral("updateNotification"), QStringLiteral("qrc:/qt/qml/Rina/ui/qml/UpdateNotificationWindow.qml"), tr("新しいバージョンがあります"), 440, 260, 400, 300, false);

    if (auto *checker = qobject_cast<Rina::Core::UpdateChecker *>(m_engine->rootContext()->contextProperty(QStringLiteral("UpdateChecker")).value<QObject *>())) {
        connect(checker, &Rina::Core::UpdateChecker::updateAvailableChanged, this, [this]() -> void { setVisible(QStringLiteral("updateNotification"), true); });
    }

    // 設定の反映
    auto *bridge = m_engine->rootContext()->contextProperty(QStringLiteral("TimelineBridge")).value<QObject *>();
    if (bridge != nullptr) {
        if (!path.isEmpty()) {
            QMetaObject::invokeMethod(bridge, "loadProject", Q_ARG(QString, path));
        } else {
            auto *project = bridge->property("project").value<QObject *>();
            if (project != nullptr) {
                project->setProperty("width", w);
                project->setProperty("height", h);
                project->setProperty("fps", fps);
            }
        }
    }
}

void WindowManager::spawnWindow(QQmlEngine *engine, const QString &id, const QString &urlStr, const QString &title, int w, int h, int x, int y, bool visible) { // NOLINT(bugprone-easily-swappable-parameters)
    if (engine == nullptr) {
        qWarning() << "WindowManager: QMLエンジンがnullです！";
        return;
    }

    QQmlComponent comp(engine, QUrl(urlStr));
    if (comp.status() != QQmlComponent::Ready) {
        qWarning() << "QMLエラー (" << title << "):" << comp.errorString();
        return;
    }

    QObject *obj = comp.create();
    if (auto *win = qobject_cast<QQuickWindow *>(obj)) {
        win->setTitle(title);
        win->resize(w, h);
        win->setX(x);
        win->setY(y);
        registerWindow(id, win);
        if (visible) {
            win->show();
        } else {
            win->hide();
        }
    } else {
        // QQuickWindowではなかった場合
        auto *window = new QQuickWindow();
        window->setTitle(title);
        window->resize(w, h);
        window->setX(x);
        window->setY(y);

        auto *item = qobject_cast<QQuickItem *>(obj);
        if (item != nullptr) {
            item->setParentItem(window->contentItem());
        }
        registerWindow(id, window);
        if (visible) {
            window->show();
        } else {
            window->hide();
        }
    }
}

void WindowManager::registerWindow(const QString &id, QQuickWindow *win) {
    m_windows.insert(id, win);

    // hide/showした場合の同期
    connect(win, &QQuickWindow::visibleChanged, this, [this, id]() -> void { emitVisibilityChanged(id); });
    connect(win, &QObject::destroyed, this, [this, id]() -> void {
        m_windows.remove(id);
        emitVisibilityChanged(id);
    });

    // メインが閉じられたら全終了
    if (id == QStringLiteral("main")) {
        connect(win, &QQuickWindow::closing, this, [this](QQuickCloseEvent *e) -> void {
            Q_UNUSED(e);
            requestQuit();
        });
    }

    emitVisibilityChanged(id);
}

void WindowManager::emitVisibilityChanged(const QString &id) {
    if (id == QStringLiteral("timeline")) {
        emit timelineVisibleChanged();
    }
    if (id == QStringLiteral("projectSettings")) {
        emit projectSettingsVisibleChanged();
    }
    if (id == QStringLiteral("objectSettings")) {
        emit objectSettingsVisibleChanged();
    }
    if (id == QStringLiteral("systemSettings")) {
        emit systemSettingsVisibleChanged();
    }
}

auto WindowManager::isVisible(const QString &id) const -> bool {
    QPointer<QQuickWindow> w = m_windows.value(id);
    return w ? w->isVisible() : false;
}
void WindowManager::setVisible(const QString &id, bool visible) {
    QPointer<QQuickWindow> w = m_windows.value(id);
    if (!w) {
        return;
    }
    if (visible) {
        w->show();
    } else {
        w->hide();
    }
    if (visible) {
        w->raise();
        w->requestActivate();
    }
}
void WindowManager::toggleVisible(const QString &id) { setVisible(id, !isVisible(id)); }
void WindowManager::raiseWindow(const QString &id) {
    QPointer<QQuickWindow> w = m_windows.value(id);
    if (!w) {
        return;
    }
    w->show();
    w->raise();
    w->requestActivate();
}

auto WindowManager::getWindow(const QString &id) const -> QObject * { return m_windows.value(id); }

void WindowManager::requestQuit() {
    // 全Windowを閉じる
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (it.value()) {
            it.value()->close();
        }
    }
    QCoreApplication::quit();
}

auto WindowManager::timelineVisible() const -> bool { return isVisible(QStringLiteral("timeline")); }
void WindowManager::setTimelineVisible(bool v) { setVisible(QStringLiteral("timeline"), v); }
auto WindowManager::projectSettingsVisible() const -> bool { return isVisible(QStringLiteral("projectSettings")); }
void WindowManager::setProjectSettingsVisible(bool v) { setVisible(QStringLiteral("projectSettings"), v); }
auto WindowManager::objectSettingsVisible() const -> bool { return isVisible(QStringLiteral("objectSettings")); }
void WindowManager::setObjectSettingsVisible(bool v) { setVisible(QStringLiteral("objectSettings"), v); }
auto WindowManager::systemSettingsVisible() const -> bool { return isVisible(QStringLiteral("systemSettings")); }
void WindowManager::setSystemSettingsVisible(bool v) { setVisible(QStringLiteral("systemSettings"), v); }
} // namespace Rina::UI
