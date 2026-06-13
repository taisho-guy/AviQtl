#pragma once

#include <QAbstractScrollArea>
#include <cstdint>

namespace AviQtl {
namespace UI {

class TimelineGrid;
class TimelineHeaderWidget;
class TimelineRulerWidget;

class TimelineWidget : public QAbstractScrollArea {
  Q_OBJECT
public:
  explicit TimelineWidget(QWidget *parent = nullptr);
  ~TimelineWidget() override;

  QSize minimumSizeHint() const override;
  QSize sizeHint() const override;

signals:
  // タイムラインのシークバーが動いた際に発行するシグナル
  void frameChanged(int64_t frameNumber);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  int timelineLengthFrames() const;
  int contentWidth() const;
  int contentHeight() const;
  int frameAtX(int x) const;
  int rowAtY(int y) const;
  int getGridInterval() const;
  double zoomPercentToScale(double percent) const;
  double scaleToZoomPercent(double scale) const;
  void updateScrollbars();
  void ensureCursorVisible();

  int m_layerHeight = 30;
  int m_layerCount = 128;
  int m_headerWidth = 60;
  int m_tailPaddingFrames = 120;
  int m_totalFrames = 300;
  int m_cursorFrame = 0;
  int m_selectedLayer = 0;
  double m_scale = 1.0;
  int m_gridInterval = 10;
  QString m_gridMode = QStringLiteral("Auto");
  TimelineGrid *m_grid = nullptr;
  TimelineHeaderWidget *m_header = nullptr;
  TimelineRulerWidget *m_ruler = nullptr;
  int m_rulerHeight = 32;
};

} // namespace UI
} // namespace AviQtl