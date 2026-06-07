#pragma once

#include <QWidget>

namespace AviQtl {
namespace UI {

class TimelineWidget : public QWidget {
  Q_OBJECT
public:
  explicit TimelineWidget(QWidget *parent = nullptr);
  ~TimelineWidget() override;

signals:
  // タイムラインのシークバーが動いた際に発行するシグナル
  void frameChanged(int64_t frameNumber);
};

} // namespace UI
} // namespace AviQtl