#pragma once

#include <QPainter>
#include <QString>

namespace AviQtl {
namespace UI {

struct GridSettings {
  QString mode = QStringLiteral("Auto");
  int bpm = 120;
  int offset = 0;
  int interval = 10;
  int subdivision = 4;
};

class TimelineGrid {
public:
  TimelineGrid();

  void setGridSettings(const GridSettings &s) { m_settings = s; }
  void setLayerParams(int layerCount, int layerHeight) {
    m_layerCount = layerCount;
    m_layerHeight = layerHeight;
  }
  void setTimelineDuration(int duration) { m_timelineDuration = duration; }

  // 描画エントリポイント
  void draw(QPainter &painter, int contentX, int contentY, int width,
            int height, double scale, int timelineLengthFrames,
            const QPalette &palette);

  int getGridInterval(double scale) const;

private:
  GridSettings m_settings;
  int m_layerCount = 128;
  int m_layerHeight = 30;
  int m_timelineDuration = 300;
};

} // namespace UI
} // namespace AviQtl
