#pragma once
#include <QObject>
#include <QString>

namespace Rina::Core {
class VideoEncoder;
}

namespace Rina::UI {
class TimelineController;

class TimelineExportManager : public QObject {
    Q_OBJECT
  public:
    explicit TimelineExportManager(TimelineController *controller, QObject *parent = nullptr);

    bool exportMedia(const QString &fileUrl, const QString &format, int quality);
    void exportVideoHW(Rina::Core::VideoEncoder *encoder);

  private:
    bool exportImageSequence(const QString &dir, int quality);
    bool exportVideo(const QString &path, const QString &format, int quality);
    TimelineController *m_controller;
};
} // namespace Rina::UI