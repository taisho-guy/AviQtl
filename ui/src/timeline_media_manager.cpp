#include "timeline_media_manager.hpp"
#include "../../core/include/audio_decoder.hpp"
#include "../../core/include/media_decoder.hpp"
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
    for (auto *decoder : m_decoders) {
        decoder->setPlaying(playing);
    }
}

void TimelineMediaManager::onCurrentFrameChanged() {
    // スクラブ中はUIのみの追従とし、プレビュー（映像・音声）の重いシークは行わない
    if (m_controller->transport()->isScrubbing())
        return;

    int nextFrame = m_controller->transport()->currentFrame();
    int fps = m_controller->project()->fps();

    // ループ時のシーク
    if (nextFrame == 0) {
        for (auto *decoder : m_decoders)
            decoder->seek(0);
    }

    if (m_controller->transport()->isPlaying()) {
        int sampleRate = m_controller->project()->sampleRate();
        m_audioMixer->processFrame(nextFrame, fps, sampleRate / fps);
    }

    for (auto it = m_decoders.begin(); it != m_decoders.end(); ++it) {
        const auto *clip = m_controller->timeline()->findClipById(it.key());
        if (!clip || nextFrame < clip->startFrame || nextFrame >= clip->startFrame + clip->durationFrames)
            continue;

        if (auto *vid = qobject_cast<Rina::Core::VideoDecoder *>(it.value())) {
            const int relFrame = nextFrame - clip->startFrame;
            const double relTime = static_cast<double>(relFrame) / fps;

            for (const auto *eff : clip->effects) {
                if (eff->id() != "video")
                    continue;

                const QString playMode = eff->params().value("playMode", "開始フレーム＋再生速度").toString();

                if (playMode == "フレーム直接指定") {
                    // 動画の絶対フレーム番号を直接シーク（プロジェクトFPSと無関係）
                    const int absFrame = eff->evaluatedParam("directFrame", relFrame).toInt();
                    vid->seekToFrame(absFrame, vid->sourceFps());
                } else {
                    // 開始フレームは動画側フレーム番号として秒数に変換してシーク
                    const int startFrame = eff->params().value("startFrame", 0).toInt();
                    const double speed = eff->params().value("speed", 100.0).toDouble();

                    double vfps = vid->sourceFps();
                    if (vfps <= 0.0) {
                        vfps = fps;
                    }

                    const double startSec = static_cast<double>(startFrame) / vfps;
                    const double targetSec = startSec + relTime * (speed / 100.0);
                    vid->seekToTime(targetSec);
                }
                break;
            }
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
    for (auto *decoder : m_decoders) {
        decoder->setPlaybackRate(speed);
    }
    if (m_audioMixer)
        m_audioMixer->setPlaybackSpeed(speed);
}

void TimelineMediaManager::updateAudioSampleRate() {
    int rate = m_controller->project()->sampleRate();
    if (m_audioMixer)
        m_audioMixer->setSampleRate(rate);
    for (auto *decoder : m_decoders) {
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
    QSet<int> currentClipIds;

    for (const auto &clip : clips) {
        if (clip.type != "video" && clip.type != "audio")
            continue;

        currentClipIds.insert(clip.id);
        if (m_decoders.contains(clip.id))
            continue;

        QUrl sourceUrl = getClipSourceUrl(clip);
        if (!sourceUrl.isValid() || sourceUrl.isEmpty())
            continue;

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

    for (auto it = m_decoders.begin(); it != m_decoders.end();) {
        if (!currentClipIds.contains(it.key())) {
            if (qobject_cast<Rina::Core::AudioDecoder *>(it.value()))
                m_audioMixer->unregisterDecoder(it.key());
            it.value()->deleteLater();
            it = m_decoders.erase(it);
        } else {
            ++it;
        }
    }
}
} // namespace Rina::UI