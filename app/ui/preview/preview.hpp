#pragma once

#include <QWidget>

namespace AviQtl {
namespace UI {

class VideoEditorPreviewWidget : public QWidget {
  Q_OBJECT
public:
  explicit VideoEditorPreviewWidget(QWidget *parent = nullptr);
  ~VideoEditorPreviewWidget() override;

  // タイムラインなどから呼び出されるシーク関数
  Q_INVOKABLE void seekToFrame(int64_t frameNumber);

signals:
  // システム層の実際のレンダラーへ伝えるためのシグナル
  void requestSeek(int64_t frameNumber);
};

} // namespace UI
} // namespace AviQtl