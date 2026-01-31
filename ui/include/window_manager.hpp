#pragma once
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickWindow>

namespace Rina::UI {
class WindowManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool timelineVisible READ timelineVisible WRITE setTimelineVisible NOTIFY timelineVisibleChanged)
    Q_PROPERTY(bool projectSettingsVisible READ projectSettingsVisible WRITE setProjectSettingsVisible NOTIFY projectSettingsVisibleChanged)
    Q_PROPERTY(bool objectSettingsVisible READ objectSettingsVisible WRITE setObjectSettingsVisible NOTIFY objectSettingsVisibleChanged)
    Q_PROPERTY(bool systemSettingsVisible READ systemSettingsVisible WRITE setSystemSettingsVisible NOTIFY systemSettingsVisibleChanged)

  public:
    static WindowManager &instance();
    void spawnInitialWindows(QQmlEngine *engine);
    void spawnWindow(QQmlEngine *engine, const QString &id, const QString &urlStr, const QString &title, int w, int h, int x, int y, bool visible = true);

  public slots:
    void onProjectSelected(const QString &path, int width, int height, double fps, int totalFrames);

    Q_INVOKABLE bool isVisible(const QString &id) const;
    Q_INVOKABLE void setVisible(const QString &id, bool visible);
    Q_INVOKABLE void toggleVisible(const QString &id);
    Q_INVOKABLE void raiseWindow(const QString &id);
    Q_INVOKABLE void requestQuit();

    bool timelineVisible() const;
    void setTimelineVisible(bool v);
    bool projectSettingsVisible() const;
    void setProjectSettingsVisible(bool v);
    bool objectSettingsVisible() const;
    void setObjectSettingsVisible(bool v);
    bool systemSettingsVisible() const;
    void setSystemSettingsVisible(bool v);

  signals:
    void timelineVisibleChanged();
    void projectSettingsVisibleChanged();
    void objectSettingsVisibleChanged();
    void systemSettingsVisibleChanged();

  private:
    explicit WindowManager(QObject *parent = nullptr);
    void registerWindow(const QString &id, QQuickWindow *win);
    void emitVisibilityChanged(const QString &id);

    QHash<QString, QPointer<QQuickWindow>> m_windows;
    QPointer<QQmlEngine> m_engine;
};
} // namespace Rina::UI