#pragma once

#include <QWidget>

namespace AviQtl {
namespace UI {

class TimelineRulerWidget : public QWidget {
  Q_OBJECT
public:
  explicit TimelineRulerWidget(QWidget *parent = nullptr);
  ~TimelineRulerWidget() override;

  void setFPS(int fps) {
    m_fps = fps;
    update();
  }
  void setTimeWidth(int w) {
    m_timeWidth = w;
    updateGeometry();
  }
  void setScale(double s) {
    m_scale = s;
    update();
  }
  void setOffsetX(int x) {
    m_offsetX = x;
    update();
  }
  void setHeaderWidth(int w) { m_headerWidth = w; }
  void setTimelineDuration(int d) {
    m_timelineDuration = d;
    update();
  }

signals:
  void zoomRequested(int percent);
  void scrubRequested(int frame);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  int m_fps = 60;
  int m_timeWidth = 60;
  int m_offsetX = 0;
  double m_scale = 1.0;
  int m_headerWidth = 60;
  int m_timelineDuration = 300;
};

} // namespace UI
} // namespace AviQtl
