#pragma once
#include "timeline_types.hpp"
#include <QHash>
#include <QObject>
#include <QUrl>

namespace Rina::Core {
class MediaDecoder;
class VideoFrameStore;
} // namespace Rina::Core
namespace Rina::Engine {
class AudioMixer;
}

namespace Rina::UI {
class TimelineController;

class TimelineMediaManager : public QObject {
    Q_OBJECT
  public:
    explicit TimelineMediaManager(TimelineController *controller, QObject *parent = nullptr);

    void setVideoFrameStore(Rina::Core::VideoFrameStore *store);
    void updateMediaDecoders();
    void onPlayingChanged();
    void onCurrentFrameChanged();
    void syncPlaybackSpeed();
    void updateAudioSampleRate();

    Rina::Engine::AudioMixer *audioMixer() const { return m_audioMixer; }

  private:
    QUrl getClipSourceUrl(const ClipData &clip) const;

    TimelineController *m_controller;
    Rina::Engine::AudioMixer *m_audioMixer = nullptr;
    Rina::Core::VideoFrameStore *m_videoFrameStore = nullptr;
    QHash<int, Rina::Core::MediaDecoder *> m_decoders;
};
} // namespace Rina::UI