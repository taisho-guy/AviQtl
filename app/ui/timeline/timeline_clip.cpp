#include "timeline_clip.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>

namespace AviQtl {
namespace UI {

TimelineClipWidget::TimelineClipWidget(int clipId, QWidget *parent)
    : QWidget(parent), m_clipId(clipId), m_startFrame(0), m_durationFrames(60),
      m_layer(0), m_scale(1.0), m_layerHeight(30), m_handleWidth(6),
      m_selected(false), m_dragging(false), m_resizingLeft(false),
      m_resizingRight(false) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setMouseTracking(true);
}

TimelineClipWidget::~TimelineClipWidget() = default;

void TimelineClipWidget::setModel(int startFrame, int durationFrames,
                                  int layer) {
  m_startFrame = startFrame;
  m_durationFrames = durationFrames;
  m_layer = layer;
  // position/size will be set by owner using updateGeometry
}

void TimelineClipWidget::setScale(double scale) { m_scale = scale; }
void TimelineClipWidget::setLayerHeight(int h) { m_layerHeight = h; }
void TimelineClipWidget::setSelected(bool s) {
  m_selected = s;
  update();
}

void TimelineClipWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, false);
  QColor bg =
      m_selected ? palette().highlight().color() : palette().mid().color();
  if (!m_selected)
    bg = QColor(80, 80, 80);
  p.fillRect(rect(), bg);
  QPen pen = m_selected ? QPen(palette().highlight().color(), 2)
                        : QPen(palette().midlight().color(), 1);
  p.setPen(pen);
  p.drawRect(rect().adjusted(0, 0, -1, -1));

  // title
  p.setPen(m_selected ? palette().highlightedText().color()
                      : palette().windowText().color());
  p.drawText(rect().adjusted(4, 2, -4, -2),
             Qt::AlignVCenter | Qt::TextSingleLine,
             QStringLiteral("Clip %1").arg(m_clipId));

  // resize handles
  p.fillRect(0, 0, m_handleWidth, height(), QColor(0, 0, 0, 40));
  p.fillRect(width() - m_handleWidth, 0, m_handleWidth, height(),
             QColor(0, 0, 0, 40));
}

void TimelineClipWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }
  m_pressPos = event->pos();
  m_initialStart = m_startFrame;
  m_initialDuration = m_durationFrames;
  m_initialLayer = m_layer;

  if (m_pressPos.x() <= m_handleWidth) {
    m_resizingLeft = true;
  } else if (m_pressPos.x() >= width() - m_handleWidth) {
    m_resizingRight = true;
  } else {
    m_dragging = true;
  }
  event->accept();
}

void TimelineClipWidget::mouseMoveEvent(QMouseEvent *event) {
  if (!m_dragging && !m_resizingLeft && !m_resizingRight) {
    if (event->pos().x() <= m_handleWidth)
      setCursor(Qt::SizeHorCursor);
    else if (event->pos().x() >= width() - m_handleWidth)
      setCursor(Qt::SizeHorCursor);
    else
      setCursor(Qt::OpenHandCursor);
    return QWidget::mouseMoveEvent(event);
  }

  QPoint d = event->pos() - m_pressPos;
  int deltaFrames = static_cast<int>(std::round(d.x() / m_scale));
  int deltaLayer =
      static_cast<int>(std::round(d.y() / static_cast<double>(m_layerHeight)));

  if (m_dragging) {
    // visually move; owner should re-position on commit
    move(x() + d.x(), y() + d.y());
    // emit preview movement on release only to reduce churn
  } else if (m_resizingLeft) {
    int newStart = m_initialStart + deltaFrames;
    int endFrame = m_initialStart + m_initialDuration;
    if (newStart < 0)
      newStart = 0;
    int newDur = endFrame - newStart;
    if (newDur < 1)
      newDur = 1;
    // temporary visual resize
    int newX = static_cast<int>(std::round(newStart * m_scale));
    int newW = static_cast<int>(std::round(newDur * m_scale));
    setGeometry(newX, y(), newW, height());
  } else if (m_resizingRight) {
    int newDur = m_initialDuration + deltaFrames;
    if (newDur < 1)
      newDur = 1;
    int newW = static_cast<int>(std::round(newDur * m_scale));
    setGeometry(x(), y(), newW, height());
  }
  event->accept();
}

void TimelineClipWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mouseReleaseEvent(event);
    return;
  }
  QPoint d = event->pos() - m_pressPos;
  int deltaFrames = static_cast<int>(std::round(d.x() / m_scale));
  int deltaLayer =
      static_cast<int>(std::round(d.y() / static_cast<double>(m_layerHeight)));

  if (m_dragging) {
    emit clipMoved(m_clipId, deltaLayer, deltaFrames, m_durationFrames);
  } else if (m_resizingLeft) {
    int newStart = m_initialStart + deltaFrames;
    int deltaStart = newStart - m_initialStart;
    int newDur = m_initialDuration - deltaStart;
    if (newDur < 1)
      newDur = 1;
    emit clipResized(m_clipId, deltaStart, newDur - m_initialDuration, 0);
  } else if (m_resizingRight) {
    int deltaDur = deltaFrames;
    if (deltaDur == 0) {
      // no-op
    }
    emit clipResized(m_clipId, 0, deltaDur, 0);
  }

  m_dragging = m_resizingLeft = m_resizingRight = false;
  event->accept();
}

void TimelineClipWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    emit clipDoubleClicked(m_clipId);
  }
  QWidget::mouseDoubleClickEvent(event);
}

} // namespace UI
} // namespace AviQtl
