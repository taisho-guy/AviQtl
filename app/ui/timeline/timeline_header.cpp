#include "timeline_header.hpp"

#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <algorithm>

namespace AviQtl {
namespace UI {

TimelineHeaderWidget::TimelineHeaderWidget(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setFixedWidth(m_headerWidth);
}

TimelineHeaderWidget::~TimelineHeaderWidget() = default;

void TimelineHeaderWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)
  QPainter p(this);
  p.fillRect(rect(), palette().button());

  int startY = m_contentY;
  for (int i = 0; i < m_layerCount; ++i) {
    int ly = i * m_layerHeight - startY;
    if (ly + m_layerHeight < 0 || ly > height())
      continue;
    QRect r(0, ly, width(), m_layerHeight);
    QColor base =
        (i % 2 == 0)
            ? palette().button().color()
            : QColor::fromRgbF(palette().button().color().redF() * 0.9,
                               palette().button().color().greenF() * 0.9,
                               palette().button().color().blueF() * 0.9);
    p.fillRect(r, base);

    if (i == m_selectedLayer) {
      QColor highlight = palette().highlight().color();
      highlight.setAlphaF(0.12);
      p.fillRect(r, highlight);
    }

    // layer number
    p.setPen(palette().text().color());
    p.drawText(r.adjusted(4, 0, -4, 0), Qt::AlignCenter,
               QString::number(i + 1));
  }
}

void TimelineHeaderWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    int y = event->pos().y() + m_contentY;
    int layer = y / m_layerHeight;
    if (layer >= 0 && layer < m_layerCount) {
      emit layerSelected(layer);
      m_selectedLayer = layer;
      update();
    }
  } else if (event->button() == Qt::RightButton) {
    int y = event->pos().y() + m_contentY;
    int layer = y / m_layerHeight;
    if (layer < 0 || layer >= m_layerCount)
      return;
    QMenu menu(this);
    menu.addAction(tr("Insert layer above"), [this, layer]() {
      emit requestInsertLayers(layer, 1, true);
    });
    menu.addAction(tr("Insert layer below"), [this, layer]() {
      emit requestInsertLayers(layer, 1, false);
    });
    menu.addSeparator();
    menu.addAction(tr("Toggle visibility"),
                   [this, layer]() { emit toggleLayerVisible(layer); });
    menu.addAction(tr("Toggle lock"),
                   [this, layer]() { emit toggleLayerLock(layer); });
    menu.exec(mapToGlobal(event->pos()));
  }
}

void TimelineHeaderWidget::wheelEvent(QWheelEvent *event) {
  int dy = event->angleDelta().y();
  if (event->pixelDelta().y() != 0)
    dy = event->pixelDelta().y() * 10;
  int nextY = m_contentY - dy;
  int maxY = std::max(0, m_layerCount * m_layerHeight - height());
  setContentY(std::clamp(nextY, 0, maxY));
  // propagate to parent scrollbar if present
  if (auto *p = parentWidget()) {
    if (auto *sc = p->findChild<QScrollBar *>()) {
      sc->setValue(m_contentY);
    }
  }
  event->accept();
}

} // namespace UI
} // namespace AviQtl
