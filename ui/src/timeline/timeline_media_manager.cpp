#include "timeline_media_manager.hpp"
#include "audio_decoder.hpp"
#include "effect_registry.hpp"
#include "engine/audio_mixer.hpp"
#include "image_decoder.hpp"
#include "media_decoder.hpp"
#include "timeline_controller.hpp"
#include "video_decoder.hpp"
#include "video_frame_store.hpp"
#include <algorithm>
#include <cmath>

namespace Rina::UI {

TimelineMediaManager::TimelineMediaManager(TimelineController *controller, QObject *parent) : QObject(parent), m_controller(controller), m_audioMixer(new Rina::Engine::AudioMixer(this)) {}

void TimelineMediaManager::setVideoFrameStore(Rina::Core::VideoFrameStore *store) {
    m_videoFrameStore = store;
    updateMediaDecoders();
}

void TimelineMediaManager::onPlayingChanged() {
    bool playing = m_controller->transport()->isPlaying();
    for (const auto &decoder : std::as_const(m_decoders)) {
        if (!decoder) {
            continue;
        }
        decoder->setPlaying(playing);
    }
}

void TimelineMediaManager::onCurrentFrameChanged() {

    int nextFrame = m_controller->transport()->currentFrame();
    double fps = m_controller->project()->fps();
    if (m_controller->transport()->isPlaying()) {
        int sampleRate = m_controller->project()->sampleRate();
        m_audioMixer->processFrame(nextFrame, fps, static_cast<int>(std::round(static_cast<double>(sampleRate) / fps)));
    }

    const auto *seekSnap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    for (auto it = m_decoders.begin(); it != m_decoders.end(); ++it) {
        const int seekCid = it.key();
        const auto *seekTr = seekSnap->transforms.find(seekCid);
        const auto *seekFx = seekSnap->effectStacks.find(seekCid);
        if (seekTr == nullptr || nextFrame < seekTr->startFrame || nextFrame >= seekTr->startFrame + seekTr->durationFrames) {
            continue;
        }
        const int relFrame = nextFrame - seekTr->startFrame;

        if (auto *vid = qobject_cast<Rina::Core::VideoDecoder *>(it.value())) {
            updateVideoClipFrameECS(vid, seekCid, relFrame, fps);
        }

        if (auto *img = qobject_cast<Rina::Core::ImageDecoder *>(it.value())) {
            img->seek(0); // 描画を強制
        }

        if (auto *aud = qobject_cast<Rina::Core::AudioDecoder *>(it.value())) {
            const double relTime = static_cast<double>(relFrame) / fps;
            double audioTime = 0.0;
            if (seekFx) {
                for (const auto &eff : std::as_const(seekFx->effects)) {
                    if (eff.id != QStringLiteral("audio")) {
                        continue;
                    }
                    const QString playMode = eff.params.value(QStringLiteral("playMode"), "開始時間＋再生速度").toString();
                    if (playMode == QStringLiteral("時間直接指定")) {
                        audioTime = eff.params.value(QStringLiteral("directTime"), 0.0).toDouble();
                    } else {
                        const double startTime = eff.params.value(QStringLiteral("startTime"), 0.0).toDouble();
                        const double speed = eff.params.value(QStringLiteral("speed"), 100.0).toDouble();
                        audioTime = (relTime * (speed / 100.0)) + startTime;
                    }
                    break;
                }
            }
            aud->seek(static_cast<qint64>(audioTime * 1000.0));
        }
    }
}

void TimelineMediaManager::syncPlaybackSpeed() {
    double speed = m_controller->transport()->playbackSpeed();
    for (const auto &decoder : std::as_const(m_decoders)) {
        if (!decoder) {
            continue;
        }
        decoder->setPlaybackRate(speed);
    }
    if (m_audioMixer) {
        m_audioMixer->setPlaybackSpeed(speed);
    }
}

void TimelineMediaManager::updateAudioSampleRate() {
    int rate = m_controller->project()->sampleRate();
    if (m_audioMixer) {
        m_audioMixer->setSampleRate(rate);
    }
    for (const auto &decoder : std::as_const(m_decoders)) {
        if (!decoder) {
            continue;
        }
        decoder->setSampleRate(rate);
    }
}

auto TimelineMediaManager::getClipSourceUrl(int clipId) -> QUrl {
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    const auto *m = snap->metadataStates.find(clipId);
    const auto *fx = snap->effectStacks.find(clipId);
    if (m == nullptr || fx == nullptr) {
        return {};
    }

    for (const auto &eff : std::as_const(fx->effects)) {
        if (eff.id == m->type) {
            const QString key = (m->type == QStringLiteral("audio")) ? QLatin1String("source") : QLatin1String("path");
            const QString path = eff.params.value(key).toString();
            return QUrl::fromLocalFile(path);
        }
    }
    return {};
}

void TimelineMediaManager::updateMediaDecoders() {
    // ECS スナップショットを正本として走査する
    QSet<int> currentClipIds;
    QHash<int, int> clipToScene;

    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    for (const auto &ct : snap->transforms) {
        const auto *m = snap->metadataStates.find(ct.clipId);
        if (m == nullptr) {
            continue;
        }
        if (m->type != QLatin1String("video") && m->type != QLatin1String("audio") && m->type != QStringLiteral("image")) {
            continue;
        }

        // 診断ログ
        if (m->type == QStringLiteral("video")) {
            QUrl diagUrl = getClipSourceUrl(ct.clipId);
            const auto *fxDbg = snap->effectStacks.find(ct.clipId);
            const int fxCount = (fxDbg != nullptr) ? fxDbg->effects.size() : 0;
            qDebug() << "[TMM] updateMediaDecoders: clipId=" << ct.clipId << "type=" << m->type << "effects.size()=" << fxCount << "sourceUrl=" << diagUrl;
            if (fxDbg != nullptr) {
                for (const auto &eff : std::as_const(fxDbg->effects)) {
                    qDebug() << "[TMM]   eff.id=" << eff.id << "params[path]=" << eff.params.value(QStringLiteral("path"));
                }
            }
        }

        currentClipIds.insert(ct.clipId);
        // sceneId は MetadataComponent フェーズ3追加予定フィールド。現時点では -1 固定。
        clipToScene.insert(ct.clipId, -1);

        QUrl sourceUrl = getClipSourceUrl(ct.clipId);
        if (!sourceUrl.isValid() || sourceUrl.isEmpty()) {
            if (m_decoders.contains(ct.clipId)) {
                if (qobject_cast<Rina::Core::AudioDecoder *>(m_decoders.value(ct.clipId)) != nullptr) {
                    m_audioMixer->unregisterDecoder(ct.clipId);
                }
                if (m_decoders.value(ct.clipId)) {
                    m_decoders.value(ct.clipId)->deleteLater();
                }
                m_decoders.remove(ct.clipId);
            }
            continue;
        }

        if (m_decoders.contains(ct.clipId)) {
            Rina::Core::MediaDecoder *existingDecoder = m_decoders.value(ct.clipId);
            // If the source has changed, we must recreate the decoder
            if (existingDecoder->source() != sourceUrl) {
                if (qobject_cast<Rina::Core::AudioDecoder *>(existingDecoder) != nullptr) {
                    m_audioMixer->unregisterDecoder(ct.clipId);
                }
                if (existingDecoder != nullptr) {
                    existingDecoder->deleteLater();
                }
                m_decoders.remove(ct.clipId);
            } else {
                continue;
            }
        }

        Rina::Core::MediaDecoder *decoder = nullptr;
        if (m->type == QStringLiteral("video")) {
            if (m_videoFrameStore == nullptr) {
                continue;
            }
            decoder = new Rina::Core::VideoDecoder(ct.clipId, sourceUrl, m_videoFrameStore, this);
        } else if (m->type == QStringLiteral("image")) {
            if (m_videoFrameStore == nullptr) {
                continue;
            }
            decoder = new Rina::Core::ImageDecoder(ct.clipId, sourceUrl, m_videoFrameStore, this);
        } else if (m->type == QStringLiteral("audio")) {
            decoder = new Rina::Core::AudioDecoder(ct.clipId, sourceUrl, this);
            if (auto *audioDecoder = qobject_cast<Rina::Core::AudioDecoder *>(decoder)) {
                m_audioMixer->registerDecoder(ct.clipId, audioDecoder);
            }
        }

        if (decoder != nullptr) {
            m_decoders.insert(ct.clipId, decoder);
            int cid = ct.clipId;
            // 画像や動画のデコード準備ができたらUIへ通知する
            connect(decoder, &Rina::Core::MediaDecoder::ready, this, [this, cid]() -> void { emit frameUpdated(cid); });

            if (auto *vid = qobject_cast<Rina::Core::VideoDecoder *>(decoder)) {
                connect(decoder, &Rina::Core::MediaDecoder::frameReady, this, [this, cid](int) -> void { emit frameUpdated(cid); });
                // 動画メタ情報が揃った時点でクリップの最大長をクランプする
                connect(vid, &Rina::Core::VideoDecoder::videoMetaReady, this, [this, cid](int totalFrameCount, double sourceFps) -> void {
                    const auto *ecsMeta = Rina::Engine::Timeline::ECS::instance().getSnapshot()->metadataStates.find(cid);
                    const auto *ecsFx = Rina::Engine::Timeline::ECS::instance().getSnapshot()->effectStacks.find(cid);
                    const auto *ecsTr = Rina::Engine::Timeline::ECS::instance().getSnapshot()->transforms.find(cid);
                    if (!ecsMeta || ecsMeta->type != QStringLiteral("video")) {
                        return;
                    }

                    int startVideoFrame = 0;
                    double speed = 100.0;
                    if (ecsFx) {
                        for (const auto &eff : std::as_const(ecsFx->effects)) {
                            if (eff.id != QStringLiteral("video")) {
                                continue;
                            }
                            const QString playMode = eff.params.value(QStringLiteral("playMode"), "開始フレーム＋再生速度").toString();
                            if (playMode == QStringLiteral("フレーム直接指定")) {
                                return;
                            }
                            startVideoFrame = eff.params.value(QStringLiteral("startFrame"), 0).toInt();
                            speed = eff.params.value(QStringLiteral("speed"), 100.0).toDouble();
                            break;
                        }
                    }

                    if (speed <= 0.0 || sourceFps <= 0.0) {
                        return;
                    }

                    const int projectFps = static_cast<int>(m_controller->project()->fps());
                    const double startSec = static_cast<double>(startVideoFrame) / sourceFps;
                    const double remainingSec = (static_cast<double>(totalFrameCount) / sourceFps) - startSec;
                    if (remainingSec <= 0.0) {
                        return;
                    }

                    const int maxDuration = static_cast<int>(remainingSec / (speed / 100.0) * projectFps);
                    if (ecsTr && maxDuration > 0 && ecsTr->durationFrames > maxDuration) {
                        m_controller->updateClip(cid, ecsTr->layer, ecsTr->startFrame, maxDuration);
                    }
                });
            }
            decoder->scheduleStart(); // 非同期起動
        }
    }

    for (auto it = m_decoders.begin(); it != m_decoders.end();) {
        if (!currentClipIds.contains(it.key())) {
            if (qobject_cast<Rina::Core::AudioDecoder *>(it.value()) != nullptr) {
                m_audioMixer->unregisterDecoder(it.key());
            }
            if (m_videoFrameStore != nullptr) {
                // キー形式を ImageDecoder 等と統一 (clipId のみを使用)
                m_videoFrameStore->invalidateFrame(QString::number(it.key()));
            }
            if (it.value()) {
                it.value()->deleteLater();
            }
            it = m_decoders.erase(it);
        } else {
            ++it;
        }
    }
}

// Phase4 廃止予定: evaluatedParam のキーフレーム評価が ClipData* 依存のため暫定残存
void TimelineMediaManager::updateVideoClipFrame(Rina::Core::VideoDecoder *vid, const ClipData *clip, int relFrame) {
    if ((vid == nullptr) || (clip == nullptr) || (m_controller == nullptr) || (m_controller->project() == nullptr)) {
        return;
    }

    relFrame = std::max(relFrame, 0);
    const double fps = [&]() -> double {
        const double f = m_controller->project()->fps();
        return f > 0.0 ? f : 30.0;
    }();
    const double relTime = static_cast<double>(relFrame) / fps;

    for (const auto *eff : clip->effects) {
        if ((eff == nullptr) || eff->id() != QStringLiteral("video")) {
            continue;
        }

        const QString playMode = eff->params().value(QStringLiteral("playMode"), "開始フレーム＋再生速度").toString();

        if (playMode == QStringLiteral("フレーム直接指定")) {
            const int absFrame = eff->evaluatedParam(QStringLiteral("directFrame"), relFrame, fps).toInt();
            vid->seekToFrame(absFrame, vid->sourceFps());
        } else {
            const int startFrame = eff->evaluatedParam(QStringLiteral("startFrame"), 0, fps).toInt();
            const double speed = eff->evaluatedParam(QStringLiteral("speed"), 100, fps).toDouble();

            double vfps = vid->sourceFps();
            if (vfps <= 0.0) {
                vfps = fps;
            }

            const double startSec = static_cast<double>(startFrame) / vfps;
            const double targetSec = startSec + (relTime * (speed / 100.0));
            vid->seekToTime(targetSec);
        }
        return;
    }
}

void TimelineMediaManager::updateVideoClipFrameECS(Rina::Core::VideoDecoder *vid, int clipId, int relFrame, double fps) {
    if (vid == nullptr || m_controller == nullptr) {
        return;
    }
    relFrame = std::max(relFrame, 0);
    if (fps <= 0.0) {
        fps = 30.0;
    }
    const double relTime = static_cast<double>(relFrame) / fps;

    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    const auto *fx = snap->effectStacks.find(clipId);
    if (fx == nullptr) {
        return;
    }

    for (const auto &eff : std::as_const(fx->effects)) {
        if (eff.id != QStringLiteral("video")) {
            continue;
        }
        const QString playMode = eff.params.value(QStringLiteral("playMode"), "開始フレーム＋再生速度").toString();
        if (playMode == QStringLiteral("フレーム直接指定")) {
            const int absFrame = eff.params.value(QStringLiteral("directFrame"), 0).toInt();
            vid->seekToFrame(absFrame, vid->sourceFps());
        } else {
            const int startFrame = eff.params.value(QStringLiteral("startFrame"), 0).toInt();
            const double speed = eff.params.value(QStringLiteral("speed"), 100.0).toDouble();
            double vfps = vid->sourceFps();
            if (vfps <= 0.0) {
                vfps = fps;
            }
            const double startSec = static_cast<double>(startFrame) / vfps;
            const double targetSec = startSec + (relTime * (speed / 100.0));
            vid->seekToTime(targetSec);
        }
        return;
    }
}

auto TimelineMediaManager::sceneIdForClip(int clipId) const -> int {
    // sceneId は MetadataComponent フェーズ3追加予定フィールド。現時点では -1 固定。
    Q_UNUSED(clipId)
    return -1;
}

void TimelineMediaManager::requestVideoFrame(int clipId, int relFrame) { // NOLINT(bugprone-easily-swappable-parameters)
    qDebug() << "[TMM] requestVideoFrame clipId=" << clipId << "relFrame=" << relFrame;
    if ((m_controller == nullptr) || (m_controller->timeline() == nullptr)) {
        qDebug() << "[TMM] requestVideoFrame: early return - controller/timeline null";
        return;
    }

    const auto *rvfSnap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    const auto *rvfTr = rvfSnap->transforms.find(clipId);

    if (rvfTr == nullptr) {
        qDebug() << "[TMM] requestVideoFrame: early return - clip not found clipId=" << clipId;
        return;
    }

    auto *dec = decoderForClip(clipId);
    if (dec == nullptr) {
        qDebug() << "[TMM] requestVideoFrame: decoder missing, rebuilding decoders clipId=" << clipId;
        updateMediaDecoders();
        dec = decoderForClip(clipId);
    }

    auto *vid = qobject_cast<Rina::Core::VideoDecoder *>(dec);
    qDebug() << "[TMM] requestVideoFrame: decoderForClip=" << dec << "vid=" << vid << "type=" << (dec ? dec->metaObject()->className() : "null");
    if (vid == nullptr) {
        qDebug() << "[TMM] requestVideoFrame: early return - vid null clipId=" << clipId;
        return;
    }

    updateVideoClipFrameECS(vid, clipId, relFrame, m_controller->project()->fps());
}

void TimelineMediaManager::requestImageLoad(int clipId, const QString &path) {
    if ((m_videoFrameStore == nullptr) || path.isEmpty() || clipId <= 0) {
        return;
    }

    const QUrl url = QUrl::fromLocalFile(path);

    if (m_imageDecoders.contains(clipId)) {
        auto existing = m_imageDecoders.value(clipId);
        if (existing && existing->source() == url) {
            return;
        }
    }

    auto *decoder = new Rina::Core::ImageDecoder(clipId, url, m_videoFrameStore, this);
    connect(decoder, &Rina::Core::MediaDecoder::ready, this, [this, clipId]() -> void { emit frameUpdated(clipId); });
    m_imageDecoders.insert(clipId, decoder);
    decoder->load();
}

} // namespace Rina::UI