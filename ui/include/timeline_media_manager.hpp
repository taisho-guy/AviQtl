#pragma once
#include "../../core/include/media_decoder.hpp"
#include "timeline_types.hpp"
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QUrl>

namespace Rina::Core {
class MediaDecoder;
class VideoDecoder;
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
    void requestVideoFrame(int clipId, int relFrame);

    Rina::Engine::AudioMixer *audioMixer() const { return m_audioMixer; }

    // 波形取得用: クリップIDに対応するデコーダを返す
    Rina::Core::MediaDecoder *decoderForClip(int clipId) const { return m_decoders.value(clipId, nullptr); }

  signals:
    void frameUpdated(int clipId);

  private:
    QUrl getClipSourceUrl(const ClipData &clip) const;
    void updateVideoClipFrame(Rina::Core::VideoDecoder *vid, const ClipData *clip, int relFrame);
    int sceneIdForClip(int clipId) const;

    TimelineController *m_controller;
    QPointer<Rina::Engine::AudioMixer> m_audioMixer;
    Rina::Core::VideoFrameStore *m_videoFrameStore = nullptr;
    QHash<int, QPointer<Rina::Core::MediaDecoder>> m_decoders;
};
} // namespace Rina::UI
