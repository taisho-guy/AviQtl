#pragma once

#include <QWidget>

namespace AviQtl {
namespace UI {

class TimelineClipWidget : public QWidget {
  Q_OBJECT
public:
  explicit TimelineClipWidget(int clipId, QWidget *parent = nullptr);
  ~TimelineClipWidget() override;

  void setModel(int startFrame, int durationFrames, int layer);
  void setScale(double scale);
  void setLayerHeight(int h);
  void setSelected(bool s);

signals:
  void clipMoved(int clipId, int deltaLayer, int deltaStartFrame, int duration);
  void clipResized(int clipId, int deltaStartFrame, int deltaDuration,
                   int unused);
  void clipDoubleClicked(int clipId);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
  int m_clipId;
  int m_startFrame;
  int m_durationFrames;
  int m_layer;
  double m_scale;
  int m_layerHeight;
  int m_handleWidth;
  bool m_selected;

  // Drag state
  bool m_dragging;
  bool m_resizingLeft;
  bool m_resizingRight;
  QPoint m_pressPos;
  int m_initialStart;
  int m_initialDuration;
  int m_initialLayer;
};

} // namespace UI
} // namespace AviQtl
