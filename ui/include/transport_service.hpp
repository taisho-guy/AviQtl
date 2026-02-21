#pragma once
#include <QObject>
#include <QTimer>
#include <cmath>

namespace Rina::UI {

class TransportService : public QObject {
    Q_OBJECT
    Q_PROPERTY(int currentFrame READ currentFrame WRITE setCurrentFrame NOTIFY currentFrameChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(double playbackSpeed READ playbackSpeed WRITE setPlaybackSpeed NOTIFY playbackSpeedChanged)
    Q_PROPERTY(double fps READ fps WRITE setFps NOTIFY fpsChanged)

  public:
    explicit TransportService(QObject *parent = nullptr);

    int currentFrame() const;
    void setCurrentFrame(int f);

    bool isPlaying() const;
    Q_INVOKABLE void togglePlay();

    void updateTimerInterval(double fps);

    double playbackSpeed() const { return m_playbackSpeed; }
    void setPlaybackSpeed(double speed) {
        if (std::abs(m_playbackSpeed - speed) < 0.001)
            return;
        m_playbackSpeed = speed;
        updateTimer();
        emit playbackSpeedChanged();
    }

    double fps() const { return m_fps; }
    void setFps(double fps) {
        if (std::abs(m_fps - fps) < 0.001)
            return;
        m_fps = fps;
        updateTimer();
        emit fpsChanged();
    }

  signals:
    void currentFrameChanged();
    void isPlayingChanged();
    void playbackSpeedChanged();
    void fpsChanged();

  private:
    void updateTimer() {
        if (m_timer && m_fps > 0 && m_playbackSpeed > 0) {
            // 1フレームあたりの時間(ms) / 再生速度
            double interval = 1000.0 / (m_fps * m_playbackSpeed);
            m_timer->setInterval(std::max(1, (int)interval));
        }
    }

    int m_currentFrame = 0;
    bool m_isPlaying = false;
    double m_playbackSpeed = 1.0;
    double m_fps = 60.0;
    QTimer *m_timer;
};
} // namespace Rina::UI