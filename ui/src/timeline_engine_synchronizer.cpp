#include "timeline_engine_synchronizer.hpp"
#include "../../engine/timeline/ecs.hpp"
#include "clip_model.hpp"
#include "timeline_controller.hpp"
#include <QtConcurrent>
#include <algorithm>

namespace Rina::UI {

TimelineEngineSynchronizer::TimelineEngineSynchronizer(TimelineController *controller, QObject *parent) : QObject(parent), m_controller(controller) {
    m_clipModel = new ClipModel(controller->transport(), this);
    connect(&m_futureWatcher, &QFutureWatcher<ClipEngineResult>::finished, this, &TimelineEngineSynchronizer::handleResultsReady);
}

void TimelineEngineSynchronizer::updateActiveClipsList() {
    int current = m_controller->transport()->currentFrame();
    // 60フレーム(約1秒)先までを「アクティブ」として扱い、QML側でプリロードさせる
    const int lookahead = 60;
    QList<ClipData *> active = m_controller->timeline()->resolvedActiveClipsAt(current, lookahead);

    // レイヤー番号が小さいほど背面、大きいほど前面になるようソート
    std::sort(active.begin(), active.end(), [](const ClipData *a, const ClipData *b) { return a->layer > b->layer; });

    m_clipModel->updateClips(active);
    updateECSState(active, current);
}

void TimelineEngineSynchronizer::updateECSState(const QList<ClipData *> &activeClips, int currentFrame) {
    // 1. すべての計算をワーカースレッドへオフロード
    // LuaHostがスレッドローカル化されたため、ここでの並列実行はロックフリーとなる
    auto mapper = [currentFrame](const ClipData *clip) -> ClipEngineResult {
        ClipEngineResult res;
        res.clipId = clip->id;
        res.layer = clip->layer;
        res.relFrame = currentFrame - clip->startFrame;

        if (clip->type == "audio" || clip->type == "video") {
            res.hasAudio = true;
            res.startFrame = clip->startFrame;
            res.durationFrames = clip->durationFrames;

            const QString paramSourceId = clip->type; // "audio" or "video" acts as the base effect ID
            for (auto *eff : clip->effects) {
                if (eff->id() == paramSourceId) {
                    QVariant vVol = eff->evaluatedParam("volume", res.relFrame);
                    res.vol = vVol.isValid() ? vVol.toFloat() : 1.0f;
                    res.pan = eff->evaluatedParam("pan", res.relFrame).toFloat();
                    res.mute = eff->evaluatedParam("mute", res.relFrame).toBool();
                    break;
                }
            }
        }
        return res;
    };

    m_futureWatcher.setFuture(QtConcurrent::mapped(activeClips, mapper));
}

void TimelineEngineSynchronizer::handleResultsReady() {
    // 2. 計算結果をECSへコミット（ここはメインスレッド）
    const auto results = m_futureWatcher.future().results();
    for (const auto &res : results) {
        Rina::Engine::Timeline::ECS::instance().updateClipState(res.clipId, res.layer, res.relFrame);
        if (res.hasAudio) {
            Rina::Engine::Timeline::ECS::instance().updateAudioClipState(res.clipId, res.startFrame, res.durationFrames, res.vol, res.pan, res.mute);
        }
    }
    Rina::Engine::Timeline::ECS::instance().commit();
}

void TimelineEngineSynchronizer::rebuildClipIndex() {
    m_sortedClips.clear();
    m_maxDuration = 0;
    m_timelineDuration = 0;
    auto &clips = m_controller->timeline()->clipsMutable();
    m_sortedClips.reserve(clips.size());

    QSet<int> aliveIds;

    for (auto &clip : clips) {
        aliveIds.insert(clip.id);
        m_sortedClips.push_back(&clip);
        if (clip.durationFrames > m_maxDuration)
            m_maxDuration = clip.durationFrames;

        int end = clip.startFrame + clip.durationFrames;
        if (end > m_timelineDuration)
            m_timelineDuration = end;
    }
    std::sort(m_sortedClips.begin(), m_sortedClips.end(), [](const ClipData *a, const ClipData *b) { return a->startFrame < b->startFrame; });

    // ECSの不要になったコンポーネントを掃除する
    Rina::Engine::Timeline::ECS::instance().syncClipIds(aliveIds);
}
} // namespace Rina::UI