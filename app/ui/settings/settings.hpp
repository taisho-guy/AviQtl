#pragma once

#include <QWidget>

namespace AviQtl {
namespace UI {

class SettingsDialogWidget : public QWidget {
  Q_OBJECT
public:
  explicit SettingsDialogWidget(QWidget *parent = nullptr);
  ~SettingsDialogWidget() override;
};

} // namespace UI
} // namespace AviQtl