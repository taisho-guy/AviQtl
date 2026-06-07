#include "preview.hpp"

namespace AviQtl {
namespace UI {

VideoEditorPreviewWidget::VideoEditorPreviewWidget(QWidget *parent)
    : QWidget(parent) {}

VideoEditorPreviewWidget::~VideoEditorPreviewWidget() {}

void VideoEditorPreviewWidget::seekToFrame(int64_t frameNumber) {
  // システム層へシーク要求を転送
  emit requestSeek(frameNumber);
}

} // namespace UI
} // namespace AviQtl