#include "audio_waveform_item.hpp"
#include "timeline_controller.hpp"
#include <QPainter>
#include <QPainterPath>

AudioWaveformItem::AudioWaveformItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
    // アンテナモードを有効にして、QMLプロパティの変更を検知しやすくする
    setAntialiasing(true);
}

int AudioWaveformItem::clipId() const { return m_clipId; }
void AudioWaveformItem::setClipId(int id) {
    if (m_clipId != id) {
        m_clipId = id;
        emit clipIdChanged();
        update(); // 再描画を要求
    }
}

int AudioWaveformItem::pixelWidth() const { return m_pixelWidth; }
void AudioWaveformItem::setPixelWidth(int width) {
    if (m_pixelWidth != width) {
        m_pixelWidth = width;
        emit pixelWidthChanged();
        update();
    }
}

int AudioWaveformItem::displayDuration() const { return m_displayDuration; }
void AudioWaveformItem::setDisplayDuration(int duration) {
    if (m_displayDuration != duration) {
        m_displayDuration = duration;
        emit displayDurationChanged();
        update();
    }
}

QObject *AudioWaveformItem::timelineBridge() const { return m_timelineBridge; }
void AudioWaveformItem::setTimelineBridge(QObject *bridge) {
    auto *controller = qobject_cast<Rina::UI::TimelineController *>(bridge);
    if (m_timelineBridge != controller) {
        m_timelineBridge = controller;
        emit timelineBridgeChanged();
        update();
    }
}

void AudioWaveformItem::paint(QPainter *painter) {
    if (!m_timelineBridge || m_clipId < 0 || m_pixelWidth <= 0 || m_displayDuration <= 0) {
        return;
    }

    // TimelineControllerから波形データを取得
    QVariantList peaks = m_timelineBridge->getWaveformPeaks(m_clipId, m_pixelWidth, m_displayDuration);
    if (peaks.isEmpty()) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(QColor(255, 255, 255, 230), 1)); // 白に近い半透明色

    const qreal centerY = height() / 2.0;
    const qreal maxH = centerY - 2.0; // 上下のマージン

    // パフォーマンス向上のため QPainterPath を使用
    QPainterPath path;

    for (int i = 0; i < peaks.size(); ++i) {
        const qreal peakValue = peaks[i].toReal();
        const qreal h = qMax(1.0, peakValue * maxH);
        const qreal x = i + 0.5; // ピクセルの中央に線を引く
        path.moveTo(x, centerY - h);
        path.lineTo(x, centerY + h);
    }

    painter->drawPath(path);
}