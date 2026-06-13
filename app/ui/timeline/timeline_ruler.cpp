#include "timeline_ruler.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <cmath>

namespace AviQtl {
namespace UI {

TimelineRulerWidget::TimelineRulerWidget(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setFixedHeight(32);
}

TimelineRulerWidget::~TimelineRulerWidget() = default;

void TimelineRulerWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)
  QPainter p(this);
  p.fillRect(rect(), palette().window());

  if (m_scale <= 0)
    return;

  // choose interval based on scale
  int frameInterval = 60;
  if (m_scale > 5)
    frameInterval = 10;
  else if (m_scale > 1)
    frameInterval = 30;
  else if (m_scale > 0.5)
    frameInterval = 60;
  else
    frameInterval = 300;

  int viewWidth = width();
  int viewOffsetX = m_offsetX;
  int startFrame = static_cast<int>(std::floor(viewOffsetX / m_scale));
  int endFrame =
      static_cast<int>(std::ceil((viewOffsetX + viewWidth) / m_scale));
  int alignedStart = (startFrame / frameInterval) * frameInterval;

  QPen pen(palette().text().color());
  p.setPen(pen);
  QFont f = font();
  f.setPixelSize(10);
  p.setFont(f);
  for (int fFrame = alignedStart; fFrame <= endFrame; fFrame += frameInterval) {
    int pixelX = static_cast<int>(std::round(fFrame * m_scale - viewOffsetX));
    // large tick
    p.drawLine(pixelX, 15, pixelX, height());
    if (fFrame % m_fps == 0) {
      int totalSeconds = fFrame / m_fps;
      int hours = totalSeconds / 3600;
      int minutes = (totalSeconds % 3600) / 60;
      int seconds = totalSeconds % 60;
      QString timeLabel;
      if (hours > 0)
        timeLabel = QString::number(hours) + ":" +
                    QString("%1").arg(minutes, 2, 10, QChar('0')) + ":" +
                    QString("%1").arg(seconds, 2, 10, QChar('0'));
      else if (minutes > 0)
        timeLabel = QString::number(minutes) + ":" +
                    QString("%1").arg(seconds, 2, 10, QChar('0'));
      else
        timeLabel = QString::number(seconds) + "s";
      p.drawText(pixelX + 3, 12, timeLabel);
      p.setFont(QFont());
      p.setPen(QPen(QColor(128, 128, 128)));
      p.drawText(pixelX + 3, 24, QString::number(fFrame) + "f");
      p.setPen(pen);
      p.setFont(f);
    } else {
      p.drawText(pixelX + 2, 12, QString::number(fFrame));
    }
  }
}

void TimelineRulerWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    int x = event->pos().x();
    int frame =
        static_cast<int>(std::round((x + m_offsetX - m_timeWidth) / m_scale));
    if (frame < 0)
      frame = 0;
    emit scrubRequested(frame);
  }
}

void TimelineRulerWidget::wheelEvent(QWheelEvent *event) {
  int dy = event->angleDelta().y();
  if (event->pixelDelta().y() != 0)
    dy = event->pixelDelta().y() * 10;
  double zoomFactor = dy > 0 ? 1.1 : 0.9;
  // translate to percent and emit request
  double curPercent = (m_scale <= 1.0)
                          ? (m_scale * 100.0)
                          : (100.0 + ((m_scale - 1.0) * 300.0 / 9.0));
  int newPercent = static_cast<int>(std::round(curPercent * (zoomFactor)));
  emit zoomRequested(newPercent);
  event->accept();
}

} // namespace UI
} // namespace AviQtl
