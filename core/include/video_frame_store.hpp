#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>

namespace Rina::Core {

class VideoFrameStore : public QObject {
    Q_OBJECT
  public:
    explicit VideoFrameStore(QObject *parent = nullptr);
    Q_INVOKABLE void setFrame(const QString &key, const QImage &img);
    QImage frame(const QString &key) const;
  signals:
    void frameUpdated(const QString &key);

  private:
    mutable QMutex m_mutex;
    QHash<QString, QImage> m_frames;
};
} // namespace Rina::Core