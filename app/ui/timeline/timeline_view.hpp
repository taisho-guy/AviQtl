#pragma once

#include <QWidget>

namespace AviQtl {
namespace UI {

class TimelineWidget;

class TimelineView : public QWidget {
  Q_OBJECT
public:
  explicit TimelineView(QWidget *parent = nullptr);
  ~TimelineView() override;

  // simple property setters
  void setLayerHeight(int h);
  void setLayerCount(int c);
  void setClipResizeHandleWidth(int w);

  // expose internal widget for integration
  TimelineWidget *flickable() const;

signals:
  void frameChanged(int64_t frameNumber);

private:
  TimelineWidget *m_core = nullptr;
};

} // namespace UI
} // namespace AviQtl
