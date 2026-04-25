#pragma once
#include "../../core/include/media_decoder.hpp"
#include "timeline_types.hpp"
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QUrl>

namespace AviQtl::Core {
class MediaDecoder;
class VideoDecoder;
class ImageDecoder;
class VideoFrameStore;
} // namespace AviQtl::Core
namespace AviQtl::Engine {
class AudioMixer;
}

namespace AviQtl::UI {
class TimelineController;

class TimelineMediaManager : public QObject {
    Q_OBJECT
  public:
    explicit TimelineMediaManager(TimelineController *controller, QObject *parent = nullptr);

    void setVideoFrameStore(AviQtl::Core::VideoFrameStore *store);
    void updateMediaDecoders();
    void onPlayingChanged();
    void onCurrentFrameChanged();
    void syncPlaybackSpeed();
    void updateAudioSampleRate();
    void requestVideoFrame(int clipId, int relFrame);
    void requestImageLoad(int clipId, const QString &path);

    AviQtl::Engine::AudioMixer *audioMixer() const { return m_audioMixer; }

    // 波形取得用: クリップIDに対応するデコーダを返す
    AviQtl::Core::MediaDecoder *decoderForClip(int clipId) const { return m_decoders.value(clipId, nullptr); }

  signals:
    void frameUpdated(int clipId);

  private:
    static QUrl getClipSourceUrl(int clipId);
    // Phase4 廃止予定: ClipData* 依存の旧版。呼び出し元は updateVideoClipFrameECS に移行済み
    void updateVideoClipFrame(AviQtl::Core::VideoDecoder *vid, const ClipData *clip, int relFrame);
    void updateVideoClipFrameECS(AviQtl::Core::VideoDecoder *vid, int clipId, int relFrame, double fps);
    int sceneIdForClip(int clipId) const;

    TimelineController *m_controller;
    QPointer<AviQtl::Engine::AudioMixer> m_audioMixer;
    AviQtl::Core::VideoFrameStore *m_videoFrameStore = nullptr;
    QHash<int, QPointer<AviQtl::Core::MediaDecoder>> m_decoders;
    QHash<int, QPointer<AviQtl::Core::ImageDecoder>> m_imageDecoders;
};
} // namespace AviQtl::UI
