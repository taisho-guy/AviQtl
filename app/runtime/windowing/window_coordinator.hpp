#pragma once

#include <QObject>

namespace AviQtl {
namespace Runtime {

class WindowCoordinator : public QObject {
  Q_OBJECT
public:
  static WindowCoordinator &instance();

  void spawnInitialWindows();
  void shutdown();

private:
  explicit WindowCoordinator(QObject *parent = nullptr);
  ~WindowCoordinator() override;

  WindowCoordinator(const WindowCoordinator &) = delete;
  WindowCoordinator &operator=(const WindowCoordinator &) = delete;
};

} // namespace Runtime
} // namespace AviQtl
