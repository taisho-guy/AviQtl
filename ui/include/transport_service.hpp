#pragma once
#include <QObject>
#include <QTimer>

namespace Rina::UI {

class TransportService : public QObject {
    Q_OBJECT
    Q_PROPERTY(int currentFrame READ currentFrame WRITE setCurrentFrame NOTIFY currentFrameChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
  public:
    explicit TransportService(QObject *parent = nullptr);

    int currentFrame() const;
    void setCurrentFrame(int f);

    bool isPlaying() const;
    Q_INVOKABLE void togglePlay();

    void updateTimerInterval(double fps);

  signals:
    void currentFrameChanged();
    void isPlayingChanged();

  private:
    int m_currentFrame = 0;
    bool m_isPlaying = false;
    QTimer *m_timer;
};
} // namespace Rina::UI