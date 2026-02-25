#include "timeline_media_manager.hpp"
#include "../../core/include/audio_decoder.hpp"
#include "../../core/include/video_decoder.hpp"
#include "../../engine/audio_mixer.hpp"
#include "effect_registry.hpp"
#include "timeline_controller.hpp"

namespace Rina::UI {

TimelineMediaManager::TimelineMediaManager(TimelineController *controller, QObject *parent) : QObject(parent), m_controller(controller) { m_audioMixer = new Rina::Engine::AudioMixer(this); }

void TimelineMediaManager::setVideoFrameStore(Rina::Core::VideoFrameStore *store) {
    m_videoFrameStore = store;
    updateMediaDecoders();
}

void TimelineMediaManager::onPlayingChanged() {
    bool playing = m_controller->transport()->isPlaying();
    for (auto *decoder : m_videoDecoders) {
        decoder->setPlaying(playing);
    }
}

void TimelineMediaManager::onCurrentFrameChanged() {
    int nextFrame = m_controller->transport()->currentFrame();
    int fps = m_controller->project()->fps();

    // ループ時のシーク
    if (nextFrame == 0) {
        for (auto *decoder : m_audioDecoders) {
            decoder->seek(0);
        }
    }

    if (m_controller->transport()->isPlaying()) {
        int samples = 48000 / fps;
        m_audioMixer->processFrame(nextFrame, fps, samples);
    }

    for (auto it = m_videoDecoders.begin(); it != m_videoDecoders.end(); ++it) {
        const auto *clip = m_controller->timeline()->findClipById(it.key());
        if (clip && (nextFrame >= clip->startFrame && nextFrame < clip->startFrame + clip->durationFrames)) {
            it.value()->seekToFrame(nextFrame - clip->startFrame, fps);
        }
    }
}

void TimelineMediaManager::syncPlaybackSpeed() {
    double speed = m_controller->transport()->playbackSpeed();
    for (auto *decoder : m_videoDecoders) {
        decoder->setPlaybackRate(speed);
    }
    if (m_audioMixer)
        m_audioMixer->setPlaybackSpeed(speed);
}

void TimelineMediaManager::updateAudioSampleRate() {
    int rate = m_controller->project()->sampleRate();
    if (m_audioMixer)
        m_audioMixer->setSampleRate(rate);
    for (auto *decoder : m_audioDecoders) {
        decoder->setSampleRate(rate);
    }
}

QUrl TimelineMediaManager::getClipSourceUrl(const ClipData &clip) const {
    const EffectModel *effModel = nullptr;
    for (const auto *eff : clip.effects) {
        if (eff->id() == clip.type) {
            effModel = eff;
            break;
        }
    }
    if (!effModel)
        return QUrl();
    QString path = effModel->params().value(clip.type == "video" ? "path" : "source").toString();
    return QUrl::fromLocalFile(path);
}

void TimelineMediaManager::updateMediaDecoders() {
    const auto &clips = m_controller->timeline()->clips();
    QSet<int> currentVideoClipIds;
    QSet<int> currentAudioClipIds;

    for (const auto &clip : clips) {
        if (clip.type != "video" && clip.type != "audio")
            continue;

        if (clip.type == "video") {
            currentVideoClipIds.insert(clip.id);
            if (!m_videoDecoders.contains(clip.id)) {
                QUrl sourceUrl = getClipSourceUrl(clip);
                if (sourceUrl.isValid() && !sourceUrl.isEmpty()) {
                    if (!m_videoFrameStore)
                        continue;
                    auto *decoder = new Rina::Core::VideoDecoder(clip.id, sourceUrl, m_videoFrameStore, this);
                    decoder->setPlaying(m_controller->transport()->isPlaying());
                    m_videoDecoders.insert(clip.id, decoder);
                }
            }
        }
        if (clip.type == "audio") {
            currentAudioClipIds.insert(clip.id);
            if (!m_audioDecoders.contains(clip.id)) {
                QUrl sourceUrl = getClipSourceUrl(clip);
                if (sourceUrl.isValid() && !sourceUrl.isEmpty()) {
                    auto *decoder = new Rina::Core::AudioDecoder(clip.id, sourceUrl, this);
                    m_audioDecoders.insert(clip.id, decoder);
                    m_audioMixer->registerDecoder(clip.id, decoder);
                }
            }
        }
    }

    for (auto it = m_videoDecoders.begin(); it != m_videoDecoders.end();) {
        if (!currentVideoClipIds.contains(it.key())) {
            it.value()->deleteLater();
            it = m_videoDecoders.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_audioDecoders.begin(); it != m_audioDecoders.end();) {
        if (!currentAudioClipIds.contains(it.key())) {
            m_audioMixer->unregisterDecoder(it.key());
            it.value()->deleteLater();
            it = m_audioDecoders.erase(it);
        } else {
            ++it;
        }
    }
}
} // namespace Rina::UI