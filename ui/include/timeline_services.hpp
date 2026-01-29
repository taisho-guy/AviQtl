#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantMap>

namespace Rina::UI {

    // プロジェクト設定管理 (不変値・環境設定)
    class ProjectService : public QObject {
        Q_OBJECT
        Q_PROPERTY(int width READ width WRITE setWidth NOTIFY widthChanged)
        Q_PROPERTY(int height READ height WRITE setHeight NOTIFY heightChanged)
        Q_PROPERTY(double fps READ fps WRITE setFps NOTIFY fpsChanged)
        Q_PROPERTY(int totalFrames READ totalFrames WRITE setTotalFrames NOTIFY totalFramesChanged)
    public:
        explicit ProjectService(QObject* parent = nullptr) : QObject(parent) {}

        int width() const { return m_width; }
        void setWidth(int w) { if (m_width != w) { m_width = w; emit widthChanged(); } }

        int height() const { return m_height; }
        void setHeight(int h) { if (m_height != h) { m_height = h; emit heightChanged(); } }

        double fps() const { return m_fps; }
        void setFps(double f) { if (!qFuzzyCompare(m_fps, f)) { m_fps = f; emit fpsChanged(); } }

        int totalFrames() const { return m_totalFrames; }
        void setTotalFrames(int f) { if (m_totalFrames != f) { m_totalFrames = f; emit totalFramesChanged(); } }

    signals:
        void widthChanged();
        void heightChanged();
        void fpsChanged();
        void totalFramesChanged();

    private:
        int m_width = 1920;
        int m_height = 1080;
        double m_fps = 60.0;
        int m_totalFrames = 3600;
    };

    // 再生・時間管理 (Transport)
    class TransportService : public QObject {
        Q_OBJECT
        Q_PROPERTY(int currentFrame READ currentFrame WRITE setCurrentFrame NOTIFY currentFrameChanged)
        Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    public:
        explicit TransportService(QObject* parent = nullptr) : QObject(parent) {
            m_timer = new QTimer(this);
            m_timer->setTimerType(Qt::PreciseTimer);
            connect(m_timer, &QTimer::timeout, this, &TransportService::stepForward);
        }

        int currentFrame() const { return m_currentFrame; }
        void setCurrentFrame(int f) { if (m_currentFrame != f) { m_currentFrame = f; emit currentFrameChanged(); } }

        bool isPlaying() const { return m_isPlaying; }
        Q_INVOKABLE void togglePlay() {
            m_isPlaying = !m_isPlaying;
            if (m_isPlaying) m_timer->start(); else m_timer->stop();
            emit isPlayingChanged();
        }

        void updateTimerInterval(double fps) {
            if (fps > 0) m_timer->setInterval(static_cast<int>(1000.0 / fps));
        }

    private slots:
        void stepForward() { setCurrentFrame(m_currentFrame + 1); }

    signals:
        void currentFrameChanged();
        void isPlayingChanged();

    private:
        int m_currentFrame = 0;
        bool m_isPlaying = false;
        QTimer* m_timer;
    };

    // 選択状態管理
    class SelectionService : public QObject {
        Q_OBJECT
        Q_PROPERTY(int selectedClipId READ selectedClipId NOTIFY selectedClipIdChanged)
        Q_PROPERTY(QVariantMap selectedClipData READ selectedClipData NOTIFY selectedClipDataChanged)
    public:
        explicit SelectionService(QObject* parent = nullptr) : QObject(parent) {}

        int selectedClipId() const { return m_selectedClipId; }
        QVariantMap selectedClipData() const { return m_selectedClipData; }

        void select(int id, const QVariantMap& data) {
            if (m_selectedClipId != id) { m_selectedClipId = id; emit selectedClipIdChanged(); }
            m_selectedClipData = data; emit selectedClipDataChanged();
        }

    signals:
        void selectedClipIdChanged();
        void selectedClipDataChanged();

    private:
        int m_selectedClipId = -1;
        QVariantMap m_selectedClipData;
    };
}