#pragma once
#include "../../core/include/video_encoder.hpp"
#include <QObject>
#include <QString>
#include <QThread>
#include <atomic>

namespace Rina::Core {
class VideoEncoder;
}

namespace Rina::UI {
class TimelineController;

class TimelineExportManager : public QObject {
    Q_OBJECT
  public:
    explicit TimelineExportManager(TimelineController *controller, QObject *parent = nullptr);

    void exportVideoAsync(const Rina::Core::VideoEncoder::Config &config);
    void cancelExport();
    bool isExporting() const { return m_exporting.load(); }

    // 旧インターフェース互換
    bool exportMedia(const QString &fileUrl, const QString &format, int quality);

  signals:
    void exportStarted(int totalFrames);
    void exportProgressChanged(int progress, int currentFrame, int totalFrames);
    void exportFinished(bool success, const QString &message);

  private:
    void runExport(const Rina::Core::VideoEncoder::Config &config);
    bool exportImageSequence(const QString &dir, int quality);
    TimelineController *m_controller;
    QThread *m_exportThread = nullptr;
    std::atomic<bool> m_exporting{false};
    std::atomic<bool> m_cancelRequested{false};
};
} // namespace Rina::UI