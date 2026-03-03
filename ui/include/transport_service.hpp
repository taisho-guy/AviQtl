#pragma once
#include <QElapsedTimer>
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
    Q_PROPERTY(int totalFrames READ totalFrames WRITE setTotalFrames NOTIFY totalFramesChanged)
    Q_PROPERTY(bool isScrubbing READ isScrubbing WRITE setIsScrubbing NOTIFY isScrubbingChanged)

  public:
    explicit TransportService(QObject *parent = nullptr);

    int currentFrame() const;
    bool isScrubbing() const { return m_isScrubbing; }
    bool isPlaying() const;
    double playbackSpeed() const { return m_playbackSpeed; }
    double fps() const { return m_fps; }
    int totalFrames() const { return m_totalFrames; }

    void setCurrentFrame(int f);
    Q_INVOKABLE void setCurrentFrame_seek(int f); // シーク専用（起点リセット付き）
    Q_INVOKABLE void togglePlay();
    void setIsScrubbing(bool scrubbing) {
        if (m_isScrubbing == scrubbing)
            return;
        m_isScrubbing = scrubbing;
        emit isScrubbingChanged();
        if (!scrubbing) {
            emit currentFrameChanged();
        } // スクラブ完了時にプレビュー強制更新
    }
    void updateTimerInterval(double fps); // 旧 API 互換

    void setPlaybackSpeed(double speed) {
        if (std::abs(m_playbackSpeed - speed) < 0.001)
            return;
        m_playbackSpeed = speed;
        // 再生中なら起点を現在地に再設定（速度変更の不連続を防ぐ）
        if (m_isPlaying) {
            m_playStartFrame = m_currentFrame;
            m_playStartTime = m_clock.nsecsElapsed();
        }
        emit playbackSpeedChanged();
    }

    void setFps(double fps) {
        if (std::abs(m_fps - fps) < 0.001)
            return;
        m_fps = fps;
        emit fpsChanged();
    }

    void setTotalFrames(int n) {
        if (m_totalFrames == n)
            return;
        m_totalFrames = n;
        emit totalFramesChanged();
    }

  signals:
    void currentFrameChanged();
    void isPlayingChanged();
    void playbackSpeedChanged();
    void fpsChanged();
    void totalFramesChanged();
    void isScrubbingChanged();

  private slots:
    void onTick();

  private:
    int m_currentFrame = 0;
    bool m_isPlaying = false;
    bool m_isScrubbing = false;
    double m_playbackSpeed = 1.0;
    double m_fps = 60.0;
    int m_totalFrames = 0;

    // 再生クロック
    QElapsedTimer m_clock;      // 起動時からの単調増加クロック
    qint64 m_playStartTime = 0; // 再生開始時の m_clock.nsecsElapsed()
    int m_playStartFrame = 0;   // 再生開始時のフレーム番号

    QTimer *m_timer;
};
} // namespace Rina::UI