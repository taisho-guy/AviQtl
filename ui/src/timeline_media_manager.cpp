#include "timeline_media_manager.hpp"
#include "../../core/include/audio_decoder.hpp"
#include "../../core/include/media_decoder.hpp"
#include "../../core/include/video_decoder.hpp"
#include "../../core/include/video_frame_store.hpp"
#include "../../engine/audio_mixer.hpp"
#include "effect_registry.hpp"
#include "timeline_controller.hpp"
#include <algorithm>
#include <cmath>

namespace Rina::UI {

TimelineMediaManager::TimelineMediaManager(TimelineController *controller, QObject *parent) : QObject(parent), m_controller(controller) { m_audioMixer = new Rina::Engine::AudioMixer(this); }

void TimelineMediaManager::setVideoFrameStore(Rina::Core::VideoFrameStore *store) {
    m_videoFrameStore = store;
    updateMediaDecoders();
}

void TimelineMediaManager::onPlayingChanged() {
    bool playing = m_controller->transport()->isPlaying();
    for (auto decoder : m_decoders) {
        if (!decoder)
            continue;
        decoder->setPlaying(playing);
    }
}

void TimelineMediaManager::onCurrentFrameChanged() {

    int nextFrame = m_controller->transport()->currentFrame();
    double fps = m_controller->project()->fps();
    if (m_controller->transport()->isPlaying()) {
        int sampleRate = m_controller->project()->sampleRate();
        m_audioMixer->processFrame(nextFrame, fps, std::round(static_cast<double>(sampleRate) / fps));
    }

    for (auto it = m_decoders.begin(); it != m_decoders.end(); ++it) {
        const auto *clip = m_controller->timeline()->findClipById(it.key());
        if (!clip || nextFrame < clip->startFrame || nextFrame >= clip->startFrame + clip->durationFrames)
            continue;

        if (auto *vid = qobject_cast<Rina::Core::VideoDecoder *>(it.value())) {
            updateVideoClipFrame(vid, clip, nextFrame - clip->startFrame);
        }

        if (auto *aud = qobject_cast<Rina::Core::AudioDecoder *>(it.value())) {
            const int relFrame = nextFrame - clip->startFrame;
            const double relTime = static_cast<double>(relFrame) / fps;
            double audioTime = 0.0;

            for (const auto *eff : clip->effects) {
                if (eff->id() != "audio")
                    continue;

                const QString playMode = eff->params().value("playMode", "開始時間＋再生速度").toString();

                if (playMode == "時間直接指定") {
                    audioTime = eff->evaluatedParam("directTime", relFrame).toDouble();
                } else {
                    const double startTime = eff->params().value("startTime", 0.0).toDouble();
                    const double speed = eff->params().value("speed", 100.0).toDouble();
                    audioTime = relTime * (speed / 100.0) + startTime;
                }
                break;
            }
            aud->seek(static_cast<qint64>(audioTime * 1000.0));
        }
    }
}

void TimelineMediaManager::syncPlaybackSpeed() {
    double speed = m_controller->transport()->playbackSpeed();
    for (auto decoder : m_decoders) {
        if (!decoder)
            continue;
        decoder->setPlaybackRate(speed);
    }
    if (m_audioMixer)
        m_audioMixer->setPlaybackSpeed(speed);
}

void TimelineMediaManager::updateAudioSampleRate() {
    int rate = m_controller->project()->sampleRate();
    if (m_audioMixer)
        m_audioMixer->setSampleRate(rate);
    for (auto decoder : m_decoders) {
        if (!decoder)
            continue;
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
    // 巨大な QList<ClipData> のコピー作成を避け、元のデータ構造を直接走査する
    const auto &scenes = m_controller->timeline()->getAllScenes();
    QSet<int> currentClipIds;
    QHash<int, int> clipToScene;

    for (const auto &scene : scenes) {
        for (const auto &clip : scene.clips) {
            if (clip.type != "video" && clip.type != "audio")
                continue;

            currentClipIds.insert(clip.id);
            clipToScene.insert(clip.id, scene.id);

            QUrl sourceUrl = getClipSourceUrl(clip);
            if (!sourceUrl.isValid() || sourceUrl.isEmpty()) {
                if (m_decoders.contains(clip.id)) {
                    if (qobject_cast<Rina::Core::AudioDecoder *>(m_decoders[clip.id]))
                        m_audioMixer->unregisterDecoder(clip.id);
                    if (m_decoders[clip.id])
                        m_decoders[clip.id]->deleteLater();
                    m_decoders.remove(clip.id);
                }
                continue;
            }

            if (m_decoders.contains(clip.id)) {
                Rina::Core::MediaDecoder *existingDecoder = m_decoders[clip.id];
                // If the source has changed, we must recreate the decoder
                if (existingDecoder->property("sourceUrl").toUrl() != sourceUrl) {
                    if (qobject_cast<Rina::Core::AudioDecoder *>(existingDecoder))
                        m_audioMixer->unregisterDecoder(clip.id);
                    if (existingDecoder)
                        existingDecoder->deleteLater();
                    m_decoders.remove(clip.id);
                } else {
                    continue;
                }
            }

            Rina::Core::MediaDecoder *decoder = nullptr;
            if (clip.type == "video") {
                if (!m_videoFrameStore)
                    continue;
                decoder = new Rina::Core::VideoDecoder(clip.id, sourceUrl, m_videoFrameStore, this);
            } else if (clip.type == "audio") {
                decoder = new Rina::Core::AudioDecoder(clip.id, sourceUrl, this);
                if (auto *audioDecoder = qobject_cast<Rina::Core::AudioDecoder *>(decoder))
                    m_audioMixer->registerDecoder(clip.id, audioDecoder);
            }

            if (decoder) {
                decoder->setProperty("sourceUrl", sourceUrl);
                m_decoders.insert(clip.id, decoder);
                if (auto *vid = qobject_cast<Rina::Core::VideoDecoder *>(decoder)) {
                    int cid = clip.id;
                    connect(decoder, &Rina::Core::MediaDecoder::frameReady, this, [this, cid](int) { emit frameUpdated(cid); });
                    // 動画メタ情報が揃った時点でクリップの最大長をクランプする
                    connect(vid, &Rina::Core::VideoDecoder::videoMetaReady, this, [this, cid](int totalFrameCount, double sourceFps) {
                        const auto *clip = m_controller->timeline()->findClipById(cid);
                        if (!clip || clip->type != "video")
                            return;

                        int startVideoFrame = 0;
                        double speed = 100.0;
                        for (const auto *eff : clip->effects) {
                            if (eff->id() != "video")
                                continue;
                            const QString playMode = eff->params().value("playMode", "開始フレーム＋再生速度").toString();
                            if (playMode == "フレーム直接指定")
                                return;
                            startVideoFrame = eff->params().value("startFrame", 0).toInt();
                            speed = eff->params().value("speed", 100.0).toDouble();
                            break;
                        }

                        if (speed <= 0.0 || sourceFps <= 0.0)
                            return;

                        const int projectFps = m_controller->project()->fps();
                        const double startSec = static_cast<double>(startVideoFrame) / sourceFps;
                        const double remainingSec = static_cast<double>(totalFrameCount) / sourceFps - startSec;
                        if (remainingSec <= 0.0)
                            return;

                        const int maxDuration = static_cast<int>(remainingSec / (speed / 100.0) * projectFps);
                        if (maxDuration > 0 && clip->durationFrames > maxDuration) {
                            m_controller->updateClip(clip->id, clip->layer, clip->startFrame, maxDuration);
                        }
                    });
                }
                decoder->scheduleStart(); // 非同期起動
            }
        }
    }

    for (auto it = m_decoders.begin(); it != m_decoders.end();) {
        if (!currentClipIds.contains(it.key())) {
            if (qobject_cast<Rina::Core::AudioDecoder *>(it.value()))
                m_audioMixer->unregisterDecoder(it.key());
            if (m_videoFrameStore) {
                m_videoFrameStore->invalidateFrame(QString::number(clipToScene.value(it.key(), -1)) + "_" + QString::number(it.key()));
            }
            if (it.value())
                it.value()->deleteLater();
            it = m_decoders.erase(it);
        } else {
            ++it;
        }
    }
}

void TimelineMediaManager::updateVideoClipFrame(Rina::Core::VideoDecoder *vid, const ClipData *clip, int relFrame) {
    if (!vid || !clip || !m_controller || !m_controller->project())
        return;

    relFrame = std::max(relFrame, 0);
    const double fps = [&]() {
        const double f = m_controller->project()->fps();
        return f > 0.0 ? f : 30.0;
    }();
    const double relTime = static_cast<double>(relFrame) / fps;

    for (const auto *eff : clip->effects) {
        if (!eff || eff->id() != "video")
            continue;

        const QString playMode = eff->params().value("playMode", "開始フレーム＋再生速度").toString();

        if (playMode == "フレーム直接指定") {
            const int absFrame = eff->evaluatedParam("directFrame", relFrame).toInt();
            vid->seekToFrame(absFrame, vid->sourceFps());
        } else {
            const int startFrame = eff->evaluatedParam("startFrame", 0).toInt();
            const double speed = eff->evaluatedParam("speed", 100.0).toDouble();

            double vfps = vid->sourceFps();
            if (vfps <= 0.0)
                vfps = fps;

            const double startSec = static_cast<double>(startFrame) / vfps;
            const double targetSec = startSec + relTime * (speed / 100.0);
            vid->seekToTime(targetSec);
        }
        return;
    }
}

int TimelineMediaManager::sceneIdForClip(int clipId) const {
    for (const auto &scene : m_controller->timeline()->getAllScenes()) {
        for (const auto &clip : scene.clips) {
            if (clip.id == clipId)
                return scene.id;
        }
    }
    return -1;
}

void TimelineMediaManager::requestVideoFrame(int clipId, int relFrame) {
    if (!m_controller || !m_controller->timeline())
        return;

    const ClipData *targetClip = nullptr;
    const auto &scenes = m_controller->timeline()->getAllScenes();
    for (const auto &scene : scenes) {
        for (const auto &c : scene.clips) {
            if (c.id == clipId) {
                targetClip = &c;
                break;
            }
        }
        if (targetClip)
            break;
    }

    if (!targetClip)
        return;

    auto *vid = qobject_cast<Rina::Core::VideoDecoder *>(decoderForClip(clipId));
    if (!vid)
        return;

    updateVideoClipFrame(vid, targetClip, relFrame);
}

} // namespace Rina::UI