#include "timeline.hpp"
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

namespace AviQtl {
namespace UI {

TimelineWidget::TimelineWidget(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);

  auto *label =
      new QLabel(QStringLiteral("Advanced Timeline (Extension Editing)"), this);
  layout->addWidget(label);

  auto *timelineSlider = new QSlider(Qt::Horizontal, this);
  timelineSlider->setRange(0, 1000);
  layout->addWidget(timelineSlider);

  connect(timelineSlider, &QSlider::valueChanged, this, [this](int value) {
    emit frameChanged(static_cast<int64_t>(value));
  });
}

TimelineWidget::~TimelineWidget() {}

} // namespace UI
} // namespace AviQtl