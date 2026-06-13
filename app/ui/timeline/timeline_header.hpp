#pragma once

#include <QWidget>

namespace AviQtl {
namespace UI {

class TimelineHeaderWidget : public QWidget {
  Q_OBJECT
public:
  explicit TimelineHeaderWidget(QWidget *parent = nullptr);
  ~TimelineHeaderWidget() override;

  void setLayerCount(int count) {
    m_layerCount = count;
    update();
  }
  void setLayerHeight(int h) {
    m_layerHeight = h;
    updateGeometry();
  }
  void setHeaderWidth(int w) {
    m_headerWidth = w;
    updateGeometry();
  }
  void setContentY(int y) {
    m_contentY = y;
    update();
  }
  void setSelectedLayer(int layer) {
    m_selectedLayer = layer;
    update();
  }

signals:
  void layerSelected(int layer);
  void toggleLayerVisible(int layer);
  void toggleLayerLock(int layer);
  void requestInsertLayers(int layer, int count, bool above);
  void requestShiftLayers(int fromLayer, int toLayer, int delta);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  int m_headerWidth = 60;
  int m_layerHeight = 30;
  int m_layerCount = 128;
  int m_contentY = 0;
  int m_selectedLayer = -1;
};

} // namespace UI
} // namespace AviQtl
