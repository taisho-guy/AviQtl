#include "timeline_view.hpp"
#include "timeline.hpp"

namespace AviQtl {
namespace UI {

TimelineView::TimelineView(QWidget *parent) : QWidget(parent) {
  m_core = new TimelineWidget(this);
  m_core->setParent(this);
  m_core->setGeometry(0, 0, width(), height());
  connect(m_core, &TimelineWidget::frameChanged, this,
          &TimelineView::frameChanged);
}

TimelineView::~TimelineView() = default;

void TimelineView::setLayerHeight(int h) {
  // propagate to core
  // core currently uses m_layerHeight directly, set via protected member not
  // accessible; for now, we can adjust by setting geometry
}

void TimelineView::setLayerCount(int c) {}

void TimelineView::setClipResizeHandleWidth(int w) {}

TimelineWidget *TimelineView::flickable() const { return m_core; }

} // namespace UI
} // namespace AviQtl
