#include "window_manager.hpp"
#include <QQuickWindow>
#include <QQmlComponent>
#include <QQuickItem>
#include <QDebug>
#include <QCoreApplication>

namespace Rina::UI {

    // コンストラクタ定義
    WindowManager::WindowManager(QObject* parent) : QObject(parent) {}

    // 実装: シングルトンインスタンス
    WindowManager& WindowManager::instance() {
        static WindowManager inst(nullptr);
        return inst;
    }

    void WindowManager::spawnInitialWindows(QQmlEngine* engine) {
        // メインビュー: Windowルート(MainWindow)を開く（CompositeViewはItemルートのため）
        spawnWindow(engine, "main", "qrc:/qml/MainWindow.qml", "Rina Main Preview", 640, 480, 100, 100, true);
        
        // タイムライン
        spawnWindow(engine, "timeline", "qrc:/qml/TimelineWindow.qml", "Timeline", 1280, 300, 100, 600, true);

        // プロジェクト設定
        spawnWindow(engine, "projectSettings", "qrc:/qml/ProjectSettingsWindow.qml", "Project Settings", 450, 250, 800, 100, true);

        // オブジェクト設定（AviUtl風の設定ダイアログ相当）：初期は非表示で生成だけしておく
        spawnWindow(engine, "objectSettings", "qrc:/qml/SettingDialog.qml", "Object Settings", 400, 600, 800, 420, false);
    }

    void WindowManager::spawnWindow(QQmlEngine* engine,
                                    const QString& id,
                                    const QString& urlStr,
                                    const QString& title,
                                    int w, int h, int x, int y,
                                    bool visible) {
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
             win->setX(x);
             win->setY(y);
             registerWindow(id, win);
             if (visible) win->show(); else win->hide();
        } else {
            // QQuickWindowではなかった場合 (View3D直置きなど)、ウィンドウを作る
            auto window = new QQuickWindow();
            window->setTitle(title);
            window->resize(w, h);
            window->setX(x);
            window->setY(y);
            
            auto item = qobject_cast<QQuickItem*>(obj);
            if (item) {
                item->setParentItem(window->contentItem());
            }
            registerWindow(id, window);
            if (visible) window->show(); else window->hide();
        }
    }

    void WindowManager::registerWindow(const QString& id, QQuickWindow* win) {
        m_windows[id] = win;

        // ユーザーが×で閉じたり、hide/showした場合の同期
        connect(win, &QQuickWindow::visibleChanged, this, [this, id]() {
            emitVisibilityChanged(id);
        });
        connect(win, &QObject::destroyed, this, [this, id]() {
            m_windows.remove(id);
            emitVisibilityChanged(id);
        });

        // メインが閉じられたら「全終了」
        if (id == "main") {
            connect(win, &QQuickWindow::closing, this, [this](QQuickCloseEvent* e) {
                Q_UNUSED(e);
                requestQuit();
            });
        }

        emitVisibilityChanged(id);
    }

    void WindowManager::emitVisibilityChanged(const QString& id) {
        if (id == "timeline") emit timelineVisibleChanged();
        if (id == "projectSettings") emit projectSettingsVisibleChanged();
        if (id == "objectSettings") emit objectSettingsVisibleChanged();
    }

    bool WindowManager::isVisible(const QString& id) const {
        auto w = m_windows.value(id, nullptr);
        return w ? w->isVisible() : false;
    }
    void WindowManager::setVisible(const QString& id, bool visible) {
        auto w = m_windows.value(id, nullptr);
        if (!w) return;
        if (visible) w->show(); else w->hide();
        if (visible) { w->raise(); w->requestActivate(); }
    }
    void WindowManager::toggleVisible(const QString& id) { setVisible(id, !isVisible(id)); }
    void WindowManager::raiseWindow(const QString& id) {
        auto w = m_windows.value(id, nullptr);
        if (!w) return;
        w->show();
        w->raise();
        w->requestActivate();
    }

    void WindowManager::requestQuit() {
        // 全Windowを閉じる（main自身も含めてOK）
        for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
            if (it.value()) it.value()->close();
        }
        QCoreApplication::quit();
    }

    bool WindowManager::timelineVisible() const { return isVisible("timeline"); }
    void WindowManager::setTimelineVisible(bool v) { setVisible("timeline", v); }
    bool WindowManager::projectSettingsVisible() const { return isVisible("projectSettings"); }
    void WindowManager::setProjectSettingsVisible(bool v) { setVisible("projectSettings", v); }
    bool WindowManager::objectSettingsVisible() const { return isVisible("objectSettings"); }
    void WindowManager::setObjectSettingsVisible(bool v) { setVisible("objectSettings", v); }
}
