#include "timeline_engine_synchronizer.hpp"
#include "../../engine/timeline/ecs.hpp"
#include "clip_model.hpp"
#include "timeline_controller.hpp"
#include <QtConcurrent>
#include <algorithm>

namespace Rina::UI {

TimelineEngineSynchronizer::TimelineEngineSynchronizer(TimelineController *controller, QObject *parent) : QObject(parent), m_controller(controller) { m_clipModel = new ClipModel(controller->transport(), this); }

void TimelineEngineSynchronizer::updateActiveClipsList() {
    int current = m_controller->transport()->currentFrame();
    // 60フレーム(約1秒)先までを「アクティブ」として扱い、QML側でプリロードさせる
    const int lookahead = 60;
    QList<ClipData *> active = m_controller->timeline()->resolvedActiveClipsAt(current, lookahead);

    // レイヤー番号が小さいほど背面、大きいほど前面になるようソート
    std::sort(active.begin(), active.end(), [](const ClipData *a, const ClipData *b) { return a->layer > b->layer; });

    m_clipModel->updateClips(active);
    updateECSState(active, current);
    Rina::Engine::Timeline::ECS::instance().commit();
}

void TimelineEngineSynchronizer::updateECSState(const QList<ClipData *> &activeClips, int currentFrame) {
    // 計算結果を保持する一時構造体
    struct AudioParamsResult {
        int clipId;
        int startFrame;
        int durationFrames;
        float vol;
        float pan;
        bool mute;
    };

    QList<const ClipData *> clipsToProcess;

    // 1. 座標系の更新と、並列処理対象のフィルタリング
    for (const auto *clip : activeClips) {
        Rina::Engine::Timeline::ECS::instance().updateClipState(clip->id, clip->layer, currentFrame - clip->startFrame);

        if (clip->type == "audio" || clip->type == "video") {
            clipsToProcess.append(clip);
        }
    }

    // 2. Luaスクリプト評価を含むパラメータ計算をワーカースレッドへオフロード
    // LuaHostがスレッドローカル化されたため、ここでの並列実行はロックフリーとなる
    auto mapper = [currentFrame](const ClipData *clip) -> AudioParamsResult {
        float vol = 1.0f;
        float pan = 0.0f;
        bool mute = false;
        const QString paramSourceId = clip->type; // "audio" or "video" acts as the base effect ID
        for (auto *eff : clip->effects) {
            if (eff->id() == paramSourceId) {
                int relFrame = currentFrame - clip->startFrame;
                QVariant vVol = eff->evaluatedParam("volume", relFrame);
                vol = vVol.isValid() ? vVol.toFloat() : 1.0f;
                pan = eff->evaluatedParam("pan", relFrame).toFloat();
                mute = eff->evaluatedParam("mute", relFrame).toBool();
                break;
            }
        }
        return {clip->id, clip->startFrame, clip->durationFrames, vol, pan, mute};
    };

    QList<AudioParamsResult> results = QtConcurrent::blockingMapped(clipsToProcess, mapper);

    // 3. 計算結果をECSへコミット（ここはメインスレッド）
    for (const auto &res : results) {
        Rina::Engine::Timeline::ECS::instance().updateAudioClipState(res.clipId, res.startFrame, res.durationFrames, res.vol, res.pan, res.mute);
    }
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