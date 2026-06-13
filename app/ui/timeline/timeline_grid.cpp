#include "timeline_grid.hpp"

#include <algorithm>
#include <cmath>

namespace AviQtl {
namespace UI {

TimelineGrid::TimelineGrid() = default;

int TimelineGrid::getGridInterval(double scale) const {
  if (m_settings.mode == QLatin1String("Frame"))
    return m_settings.interval;
  if (m_settings.mode == QLatin1String("BPM"))
    return std::max(1, m_settings.interval);
  if (scale < 0.5)
    return 60;
  if (scale < 1.5)
    return 10;
  if (scale < 3)
    return 5;
  return 1;
}

void TimelineGrid::draw(QPainter &painter, int contentX, int contentY,
                        int width, int height, double scale,
                        int timelineLengthFrames, const QPalette &palette) {
  if (width <= 0 || height <= 0 || scale <= 0.0)
    return;

  painter.save();
  painter.translate(-contentX, -contentY);

  // 行背景と水平線
  QColor evenBg(255, 255, 255, static_cast<int>(255 * 0.02));
  QColor lineColor(128, 128, 128, static_cast<int>(255 * 0.2));
  painter.setPen(lineColor);
  for (int i = 0; i < m_layerCount; ++i) {
    int ly = i * m_layerHeight;
    if (ly + m_layerHeight < contentY || ly > contentY + height)
      continue;
    if ((i % 2) == 0) {
      painter.fillRect(0, ly, width + contentX, m_layerHeight, evenBg);
    }
    painter.drawLine(contentX, ly, contentX + width, ly);
  }

  // 垂直グリッド線
  int interval = getGridInterval(scale);
  if (interval <= 0) {
    painter.restore();
    return;
  }

  double offsetF =
      (m_settings.mode == QLatin1String("BPM")) ? (m_settings.offset) : 0;
  bool isBpm = (m_settings.mode == QLatin1String("BPM"));
  int bpmDiv = isBpm ? (scale > 3 ? 4 : scale > 1.5 ? 2 : 1) : 1;

  int startN = static_cast<int>(
      std::ceil((std::floor(contentX / scale) - offsetF) / interval));
  int endN = static_cast<int>(
      std::floor(((contentX + width) / scale - offsetF) / interval));

  for (int n = startN; n <= endN; ++n) {
    int f = static_cast<int>(offsetF + n * interval);
    int x = static_cast<int>(std::round(f * scale));

    QPen pen;
    if (isBpm) {
      bool isMeasure = (n % (m_settings.subdivision * bpmDiv) == 0);
      bool isBeat = (n % bpmDiv == 0);
      if (isMeasure) {
        pen = QPen(QColor::fromRgbF(0.5, 0.8, 1.0, 0.5));
        pen.setWidthF(1.5);
      } else if (isBeat) {
        pen = QPen(QColor(128, 128, 128, static_cast<int>(255 * 0.3)));
        pen.setWidth(1);
      } else {
        pen = QPen(QColor(128, 128, 128, static_cast<int>(255 * 0.15)));
        pen.setWidth(1);
      }
    } else {
      pen = QPen(QColor(128, 128, 128, static_cast<int>(255 * 0.15)));
      pen.setWidth(1);
    }
    painter.setPen(pen);
    painter.drawLine(x, contentY, x, contentY + height);
  }

  painter.restore();
}

} // namespace UI
} // namespace AviQtl
