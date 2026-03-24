#pragma once

#include <QQuickPaintedItem>

// Forward declaration to avoid including the full header
namespace Rina::UI {
class TimelineController;
}

class AudioWaveformItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(int clipId READ clipId WRITE setClipId NOTIFY clipIdChanged)
    Q_PROPERTY(int pixelWidth READ pixelWidth WRITE setPixelWidth NOTIFY pixelWidthChanged)
    Q_PROPERTY(int displayDuration READ displayDuration WRITE setDisplayDuration NOTIFY displayDurationChanged)
    Q_PROPERTY(QObject *timelineBridge READ timelineBridge WRITE setTimelineBridge NOTIFY timelineBridgeChanged)

  public:
    explicit AudioWaveformItem(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    int clipId() const;
    void setClipId(int id);

    int pixelWidth() const;
    void setPixelWidth(int width);

    int displayDuration() const;
    void setDisplayDuration(int duration);

    QObject *timelineBridge() const;
    void setTimelineBridge(QObject *bridge);

  signals:
    void clipIdChanged();
    void pixelWidthChanged();
    void displayDurationChanged();
    void timelineBridgeChanged();

  private:
    int m_clipId = -1;
    int m_pixelWidth = 0;
    int m_displayDuration = 0;
    Rina::UI::TimelineController *m_timelineBridge = nullptr;
};