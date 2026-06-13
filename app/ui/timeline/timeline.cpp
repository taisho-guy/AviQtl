#include "timeline.hpp"

#include "timeline_grid.hpp"
#include "timeline_header.hpp"
#include "timeline_ruler.hpp"
#include <QApplication>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace AviQtl {
namespace UI {

static constexpr int kScrollBarThickness = 14;
static constexpr int kPlayheadWidth = 2;
static constexpr int kTickHeight = 8;
static constexpr int kLargeTickHeight = 14;

TimelineWidget::TimelineWidget(QWidget *parent) : QAbstractScrollArea(parent) {
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);

  verticalScrollBar()->setSingleStep(m_layerHeight);
  verticalScrollBar()->setPageStep(m_layerHeight * 6);
  horizontalScrollBar()->setSingleStep(60);
  horizontalScrollBar()->setPageStep(240);
  updateScrollbars();

  // Grid helper
  m_grid = new TimelineGrid();
  m_grid->setLayerParams(m_layerCount, m_layerHeight);
  GridSettings gs;
  gs.mode = QLatin1String("Auto");
  gs.bpm = 120;
  gs.offset = 0;
  gs.interval = m_gridInterval;
  gs.subdivision = 4;
  m_grid->setGridSettings(gs);

  // Header
  m_header = new TimelineHeaderWidget(this);
  m_header->setLayerCount(m_layerCount);
  m_header->setLayerHeight(m_layerHeight);
  m_header->setHeaderWidth(m_headerWidth);
  connect(m_header, &TimelineHeaderWidget::layerSelected, this,
          [this](int layer) {
            m_selectedLayer = layer;
            viewport()->update();
          });
  connect(m_header, &TimelineHeaderWidget::toggleLayerVisible, this,
          [this](int layer) {
            // TODO: connect to Workspace/currentTimeline when available
          });
  connect(m_header, &TimelineHeaderWidget::toggleLayerLock, this,
          [this](int layer) {
            // TODO: connect to Workspace/currentTimeline when available
          });

  // sync vertical scrollbar <-> header
  connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int v) {
    if (m_header)
      m_header->setContentY(v);
  });

  // Ruler
  m_ruler = new TimelineRulerWidget(this);
  m_ruler->setFPS(60);
  m_ruler->setTimeWidth(60);
  m_ruler->setScale(m_scale);
  connect(
      m_ruler, &TimelineRulerWidget::zoomRequested, this, [this](int percent) {
        double oldScale = m_scale;
        double newScale = zoomPercentToScale(percent);
        // compute anchor at center of viewport
        int mouseX = viewport()->width() / 2;
        int anchorFrame =
            frameAtX(horizontalScrollBar()->value() + mouseX - m_headerWidth);
        m_scale = newScale;
        updateScrollbars();
        int newContentX = static_cast<int>(
            std::round(anchorFrame * m_scale - mouseX + m_headerWidth));
        horizontalScrollBar()->setValue(
            std::clamp(newContentX, 0, contentWidth() - viewport()->width()));
        viewport()->update();
      });
  connect(m_ruler, &TimelineRulerWidget::scrubRequested, this,
          [this](int frame) { emit frameChanged(frame); });
}

TimelineWidget::~TimelineWidget() { delete m_grid; }

QSize TimelineWidget::minimumSizeHint() const { return QSize(320, 180); }

QSize TimelineWidget::sizeHint() const { return QSize(800, 260); }

void TimelineWidget::resizeEvent(QResizeEvent *event) {
  QAbstractScrollArea::resizeEvent(event);
  // Position header at left and adjust viewport
  // layout: header on left (below ruler), ruler on top (to right of header),
  // viewport in remaining area
  if (m_header) {
    m_header->setGeometry(0, m_rulerHeight, m_headerWidth,
                          height() - m_rulerHeight);
  }
  if (m_ruler) {
    m_ruler->setGeometry(m_headerWidth, 0, width() - m_headerWidth,
                         m_rulerHeight);
  }
  viewport()->setGeometry(m_headerWidth, m_rulerHeight, width() - m_headerWidth,
                          height() - m_rulerHeight);
  updateScrollbars();
}

void TimelineWidget::wheelEvent(QWheelEvent *event) {
  if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) {
    double zoomPercent = scaleToZoomPercent(m_scale);
    int delta = event->angleDelta().y() != 0 ? event->angleDelta().y()
                                             : event->pixelDelta().y();
    int step = 10;
    double direction = delta > 0 ? 1.0 : -1.0;
    double newScale = zoomPercentToScale(
        std::clamp(zoomPercent + direction * step, 10.0, 400.0));

    int cursorX = event->position().x();
    int frameAtCursor = frameAtX(cursorX + horizontalScrollBar()->value());
    m_scale = newScale;
    updateScrollbars();

    int newContentX = frameAtCursor * m_scale - cursorX;
    horizontalScrollBar()->setValue(
        std::clamp(newContentX, 0, contentWidth() - viewport()->width()));
    viewport()->update();
  } else if (event->modifiers() & Qt::ShiftModifier) {
    int delta = event->angleDelta().y() != 0 ? event->angleDelta().y()
                                             : event->pixelDelta().y();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta);
  } else {
    int delta = event->angleDelta().y() != 0 ? event->angleDelta().y()
                                             : event->pixelDelta().y();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta);
  }
  event->accept();
}

void TimelineWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    int frame =
        frameAtX(event->position().x() + horizontalScrollBar()->value());
    m_cursorFrame = frame;
    emit frameChanged(frame);
    ensureCursorVisible();
    viewport()->update();
  }
  QAbstractScrollArea::mousePressEvent(event);
}

void TimelineWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)

  QPainter painter(viewport());
  painter.fillRect(viewport()->rect(), palette().base());
  painter.setRenderHint(QPainter::Antialiasing, false);

  const int visibleWidth = viewport()->width();
  const int visibleHeight = viewport()->height();
  const int offsetX = horizontalScrollBar()->value();
  const int offsetY = verticalScrollBar()->value();

  const QRect contentRect(-offsetX, -offsetY, contentWidth(), contentHeight());
  painter.save();
  painter.translate(-offsetX, -offsetY);

  // sync header and ruler with viewport offsets
  if (m_header) {
    m_header->setContentY(offsetY);
    m_header->setSelectedLayer(m_selectedLayer);
  }
  if (m_ruler) {
    m_ruler->setOffsetX(offsetX + m_headerWidth);
    m_ruler->setScale(m_scale);
  }

  // background grid (delegated to TimelineGrid)
  if (m_grid) {
    m_grid->setLayerParams(m_layerCount, m_layerHeight);
    m_grid->setTimelineDuration(m_totalFrames);
    m_grid->draw(painter, offsetX, offsetY, visibleWidth, visibleHeight,
                 m_scale, timelineLengthFrames(), palette());
  }

  // selected layer highlight
  QColor highlight = palette().highlight().color();
  highlight.setAlphaF(0.08);
  painter.fillRect(0, m_selectedLayer * m_layerHeight, contentWidth(),
                   m_layerHeight, highlight);

  // ruler and ticks
  int interval = getGridInterval();
  painter.setPen(palette().text().color());
  painter.setFont(font());
  for (int frame = 0; frame <= timelineLengthFrames();
       frame += std::max(1, interval)) {
    int x = static_cast<int>(std::round(frame * m_scale));
    int tick = frame % (interval * 10) == 0 ? kLargeTickHeight : kTickHeight;
    painter.drawLine(x, 0, x, tick);
    if (frame % (interval * 10) == 0) {
      painter.drawText(QPoint(x + 4, kLargeTickHeight + 14),
                       QString::number(frame));
    }
  }

  // playhead line
  int playheadX = static_cast<int>(std::round(m_cursorFrame * m_scale));
  painter.fillRect(playheadX, 0, kPlayheadWidth, contentHeight(),
                   palette().highlight());

  painter.restore();
}

int TimelineWidget::timelineLengthFrames() const {
  return std::max(100,
                  std::max(m_totalFrames, m_totalFrames + m_tailPaddingFrames));
}

int TimelineWidget::contentWidth() const {
  return static_cast<int>(std::ceil(timelineLengthFrames() * m_scale));
}

int TimelineWidget::contentHeight() const {
  return m_layerCount * m_layerHeight;
}

int TimelineWidget::frameAtX(int x) const {
  return std::max(0, static_cast<int>(std::round(x / m_scale)));
}

int TimelineWidget::rowAtY(int y) const {
  return std::clamp(y / m_layerHeight, 0, m_layerCount - 1);
}

int TimelineWidget::getGridInterval() const {
  if (m_gridMode == QLatin1String("Frame"))
    return m_gridInterval;
  if (m_gridMode == QLatin1String("BPM"))
    return std::max(1, m_gridInterval);
  if (m_scale < 0.5)
    return 60;
  if (m_scale < 1.5)
    return 10;
  if (m_scale < 3)
    return 5;
  return 1;
}

double TimelineWidget::zoomPercentToScale(double percent) const {
  if (percent <= 100.0)
    return percent / 100.0;
  return 1.0 + ((percent - 100.0) * 9.0 / 300.0);
}

double TimelineWidget::scaleToZoomPercent(double scale) const {
  if (scale <= 1.0)
    return scale * 100.0;
  return 100.0 + ((scale - 1.0) * 300.0 / 9.0);
}

void TimelineWidget::updateScrollbars() {
  horizontalScrollBar()->setRange(
      0, std::max(0, contentWidth() - viewport()->width()));
  horizontalScrollBar()->setPageStep(viewport()->width());
  verticalScrollBar()->setRange(
      0, std::max(0, contentHeight() - viewport()->height()));
  verticalScrollBar()->setPageStep(viewport()->height());
}

void TimelineWidget::ensureCursorVisible() {
  int cursorX = static_cast<int>(std::round(m_cursorFrame * m_scale));
  int left = horizontalScrollBar()->value();
  int right = left + viewport()->width();
  if (cursorX < left || cursorX > right) {
    horizontalScrollBar()->setValue(
        std::clamp(cursorX - viewport()->width() / 2, 0,
                   contentWidth() - viewport()->width()));
  }
}

} // namespace UI
} // namespace AviQtl