#include "window_manager.hpp"
#include "settings_manager.hpp"
#include "workspace.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml>

namespace AviQtl::UI {

WindowManager::WindowManager(QObject *parent) : QObject(parent) {}

auto WindowManager::instance() -> WindowManager & {
    static WindowManager inst(nullptr);
    return inst;
}

static void ensureWindowCreated(QQmlEngine *engine, QHash<QString, QPointer<QQuickWindow>> &windows, const QString &id) {
    if (auto it = windows.find(id); it != windows.end() && !it.value().isNull()) {
        return;
    }
    if (!engine)
        return;

    // 保存された位置・サイズを取得。なければデフォルト値を使用
    QVariantMap geo = Core::SettingsManager::instance().value("windowGeometry_" + id).toMap();
    auto get = [&](const QString &k, int def) { return geo.contains(k) ? geo[k].toInt() : def; };
    bool maximized = geo.value("maximized", false).toBool();

    // 各ウィンドウの遅延生成パラメータを集中定義
    if (id == QStringLiteral("main")) {
        WindowManager::instance().spawnWindow(engine, id, QStringLiteral("qrc:/qt/qml/AviQtl/ui/qml/MainWindow.qml"), WindowManager::tr("AviQtl メインプレビュー"), get("width", 640), get("height", 480), get("x", 100), get("y", 100), true, maximized);
    } else if (id == QStringLiteral("timeline")) {
        WindowManager::instance().spawnWindow(engine, id, QStringLiteral("qrc:/qt/qml/AviQtl/ui/qml/TimelineWindow.qml"), WindowManager::tr("タイムライン"), get("width", 1280), get("height", 300), get("x", 100), get("y", 600), true, maximized);
    } else if (id == QStringLiteral("projectSettings")) {
        WindowManager::instance().spawnWindow(engine, id, QStringLiteral("qrc:/qt/qml/AviQtl/ui/qml/ProjectSettingsWindow.qml"), WindowManager::tr("プロジェクト設定"), get("width", 450), get("height", 250), get("x", 800), get("y", 100), false, maximized);
    } else if (id == QStringLiteral("objectSettings")) {
        WindowManager::instance().spawnWindow(engine, id, QStringLiteral("qrc:/qt/qml/AviQtl/ui/qml/SettingDialog.qml"), WindowManager::tr("オブジェクト設定"), get("width", 400), get("height", 600), get("x", 800), get("y", 420), false, maximized);
    } else if (id == QStringLiteral("systemSettings")) {
        WindowManager::instance().spawnWindow(engine, id, QStringLiteral("qrc:/qt/qml/AviQtl/ui/qml/SystemSettingsWindow.qml"), WindowManager::tr("環境設定"), get("width", 600), get("height", 500), get("x", 200), get("y", 200), false, maximized);
    } else if (id == QStringLiteral("about")) {
        WindowManager::instance().spawnWindow(engine, id, QStringLiteral("qrc:/qt/qml/AviQtl/ui/qml/AboutWindow.qml"), WindowManager::tr("AviQtlについて"), get("width", 400), get("height", 250), get("x", 400), get("y", 300), false, maximized);
    } else if (id == QStringLiteral("sceneSettings")) {
        WindowManager::instance().spawnWindow(engine, id, QStringLiteral("qrc:/qt/qml/AviQtl/ui/qml/SceneSettingsWindow.qml"), WindowManager::tr("シーン設定"), get("width", 450), get("height", 300), get("x", 300), get("y", 200), false, maximized);
    } else if (id == QStringLiteral("export")) {
        WindowManager::instance().spawnWindow(engine, id, QStringLiteral("qrc:/qt/qml/AviQtl/ui/qml/ExportDialog.qml"), WindowManager::tr("メディアの書き出し"), get("width", 620), get("height", 580), get("x", 240), get("y", 160), false, maximized);
    } else if (id == QStringLiteral("easingConfig")) {
        WindowManager::instance().spawnWindow(engine, id, QStringLiteral("qrc:/qt/qml/AviQtl/ui/qml/common/EasingConfigWindow.qml"), WindowManager::tr("補間設定"), get("width", 820), get("height", 540), get("x", 420), get("y", 180), false, maximized);
    } else if (id == QStringLiteral("packageManager")) {
        WindowManager::instance().spawnWindow(engine, id, QStringLiteral("qrc:/qt/qml/AviQtl/ui/qml/PackageManagerWindow.qml"), WindowManager::tr("パッケージマネージャー"), get("width", 600), get("height", 400), get("x", 500), get("y", 300), false, maximized);
    } else {
        qWarning() << "[WindowManager] Unknown lazy window id:" << id;
    }
}

void WindowManager::spawnInitialWindows(QQmlEngine *engine) {
    m_engine = engine;

    // 初期ウィンドウの生成を ensureWindowCreated に委譲
    // これにより定義が一本化され、"Unknown lazy window id" 警告も解消される
    ensureWindowCreated(engine, m_windows, QStringLiteral("main"));
    ensureWindowCreated(engine, m_windows, QStringLiteral("timeline"));

    // タブが 0 の状態で起動しているので、ランチャーを即座に表示する
    showLauncher();
}

void WindowManager::showLauncher(QQmlEngine *engine) {
    if (engine) {
        m_engine = engine;
    }

    if (m_engine == nullptr) {
        qWarning() << "showLauncher: QMLエンジンが未初期化です";
        return;
    }

    // 既存のランチャーウィンドウがあれば前面に出すだけ
    QPointer<QQuickWindow> existing = m_windows.value(QStringLiteral("launcher"));
    if (existing) {
        existing->show();
        existing->raise();
        existing->requestActivate();
        return;
    }

    QQmlComponent component(m_engine, QUrl(QStringLiteral("qrc:/qt/qml/AviQtl/ui/qml/ProjectLauncherWindow.qml")));
    if (component.status() != QQmlComponent::Ready) {
        qWarning() << "ProjectLauncherWindow コンポーネントエラー:" << component.errorString();
        return;
    }
    QObject *obj = component.create();
    auto *launcher = qobject_cast<QQuickWindow *>(obj);
    if (launcher != nullptr) {
        registerWindow(QStringLiteral("launcher"), launcher);
        launcher->show();
    } else {
        if (obj) {
            qWarning() << "ProjectLauncherWindow は QQuickWindow ではありません。実際の型:" << obj->metaObject()->className();
            delete obj;
        } else {
            qWarning() << "ProjectLauncherWindow の生成に失敗しました";
        }
    }
}

void WindowManager::spawnWindow(QQmlEngine *engine, const QString &id, const QString &urlStr, const QString &title, int w, int h, int x, int y, bool visible, bool maximized) { // NOLINT(bugprone-easily-swappable-parameters)
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
        if (maximized) {
            win->setVisibility(QWindow::Maximized);
        }

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
        if (maximized) {
            window->setVisibility(QWindow::Maximized);
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

    // 表示状態が変わった（または閉じられた）際に現在の位置を保存
    connect(win, &QQuickWindow::visibleChanged, this, [this, id, win]() -> void {
        if (!win->isVisible())
            saveWindowGeometry(id);
        emitVisibilityChanged(id);
    });
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

    // ランチャーが閉じられ、かつまだ main が生成されていなければ全終了
    if (id == QStringLiteral("launcher")) {
        connect(win, &QQuickWindow::closing, this, [this](QQuickCloseEvent *e) -> void {
            Q_UNUSED(e);
            auto mainWin = m_windows.value(QStringLiteral("main"));
            if (!mainWin || mainWin.isNull()) {
                requestQuit();
            }
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
    ensureWindowCreated(m_engine, m_windows, id);
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
    ensureWindowCreated(m_engine, m_windows, id);
    QPointer<QQuickWindow> w = m_windows.value(id);
    if (!w) {
        return;
    }
    w->show();
    w->raise();
    w->requestActivate();
}

auto WindowManager::getWindow(const QString &id) -> QObject * {
    ensureWindowCreated(m_engine, m_windows, id);
    return m_windows.value(id);
}

void WindowManager::saveWindowGeometry(const QString &id) {
    auto win = m_windows.value(id);
    if (win) {
        QVariantMap geo;
        geo["x"] = win->x();
        geo["y"] = win->y();
        geo["width"] = win->width();
        geo["height"] = win->height();
        // 最大化状態を保存 (Waylandで最も有用なレイアウト情報)
        geo["maximized"] = (win->visibility() == QWindow::Maximized);
        // SettingsManager は key が "_" で始まらなければ自動的にディスクへ保存します
        Core::SettingsManager::instance().setValue("windowGeometry_" + id, geo);
    }
}

void WindowManager::requestQuit() {
    // 全Windowを閉じる
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (it.value()) {
            saveWindowGeometry(it.key()); // 終了直前の状態を保存
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
} // namespace AviQtl::UI
