#include "settings.hpp"
#include <QLabel>
#include <QVBoxLayout>

namespace AviQtl {
namespace UI {

SettingsDialogWidget::SettingsDialogWidget(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  auto *label =
      new QLabel(QStringLiteral("AviUtl Settings Dialog (Placeholder)"), this);
  label->setAlignment(Qt::AlignCenter);
  layout->addWidget(label);
}

SettingsDialogWidget::~SettingsDialogWidget() {}

} // namespace UI
} // namespace AviQtl