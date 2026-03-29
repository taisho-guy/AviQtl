#include "timeline_engine_synchronizer.hpp"
#include "../../engine/timeline/ecs.hpp"
#include "clip_model.hpp"
#include "timeline_controller.hpp"
#include <QtConcurrent>
#include <algorithm>

namespace Rina::UI {

// Archetypeごとの計算ワークロードを定義
struct ArchetypeBatch {
    struct EffectDesc {
        int index;
        QStringList keys;
    };
    QList<ClipData *> clips;
    QList<EffectDesc> activeEffects;
    QStringList allKeys; // 結果のマッピング用
};

TimelineEngineSynchronizer::TimelineEngineSynchronizer(TimelineController *controller, QObject *parent) : QObject(parent), m_controller(controller) {
    m_clipModel = new ClipModel(controller->transport(), this);
    connect(&m_futureWatcher, &QFutureWatcher<QList<ClipEngineResult>>::finished, this, &TimelineEngineSynchronizer::handleResultsReady);
}

void TimelineEngineSynchronizer::updateActiveClipsList() {
    int current = m_controller->transport()->currentFrame();
    QList<ClipData *> active = m_controller->timeline()->resolvedActiveClipsAt(current);

    // レイヤー番号が小さいほど背面、大きいほど前面になるようソート
    std::sort(active.begin(), active.end(), [](const ClipData *a, const ClipData *b) { return a->layer > b->layer; });

    m_clipModel->updateClips(active);

    // Archetype (エフェクト構成の指紋) ごとにグループ化
    QMap<QString, ArchetypeBatch> archetypeBatches;
    for (ClipData *clip : active) {
        QString sig = clip->type;
        for (auto *eff : clip->effects) {
            if (eff->isEnabled())
                sig += ":" + eff->id();
        }

        auto &batch = archetypeBatches[sig];
        if (batch.clips.isEmpty()) {
            for (int i = 0; i < clip->effects.size(); ++i) {
                if (clip->effects[i]->isEnabled()) {
                    QStringList keys = clip->effects[i]->params().keys();
                    batch.activeEffects.append({i, keys});
                    batch.allKeys << keys;
                }
            }
        }
        batch.clips.append(clip);
    }

    updateECSState(archetypeBatches.values(), current);
}

void TimelineEngineSynchronizer::updateECSState(const QList<ArchetypeBatch> &batches, int currentFrame) {
    auto batchMapper = [currentFrame](const ArchetypeBatch &batch) -> QList<ClipEngineResult> {
        QList<ClipEngineResult> results;
        results.reserve(batch.clips.size());

        for (const ClipData *clip : batch.clips) {
            ClipEngineResult res;
            res.clipId = clip->id;
            res.layer = clip->layer;
            res.relFrame = currentFrame - clip->startFrame;
            res.propertyNames = batch.allKeys;
            res.propertyValues.reserve(batch.allKeys.size());

            // 1. エフェクト計算 (QVariantMapを介さず、名前ベースで直接値を抽出)
            for (const auto &ed : batch.activeEffects) {
                auto *eff = clip->effects[ed.index];
                for (const auto &key : ed.keys) {
                    res.propertyValues.push_back(eff->evaluatedParam(key, res.relFrame).toFloat());
                }
            }

            // 2. オーディオ/ビデオ共通属性
            if (clip->type == "audio" || clip->type == "video") {
                res.hasAudio = true;
                res.startFrame = clip->startFrame;
                res.durationFrames = clip->durationFrames;
                for (const auto &ed : batch.activeEffects) {
                    auto *eff = clip->effects[ed.index];
                    if (eff->id() == clip->type) {
                        QVariant vVol = eff->evaluatedParam("volume", res.relFrame);
                        res.vol = vVol.isValid() ? vVol.toFloat() : 1.0f;
                        res.pan = eff->evaluatedParam("pan", res.relFrame).toFloat();
                        res.mute = eff->evaluatedParam("mute", res.relFrame).toBool();
                        break;
                    }
                }
            }
            results.append(res);
        }
        return results;
    };

    m_futureWatcher.setFuture(QtConcurrent::mapped(batches, batchMapper));
}

void TimelineEngineSynchronizer::handleResultsReady() {
    // バッチ結果を ECS へコミット
    const auto batchResults = m_futureWatcher.future().results();
    m_paramCache.clear();

    for (const auto &results : batchResults) {
        for (const auto &res : results) {
            // UIスレッド用に QVariantMap を復元
            QVariantMap resMap;
            for (int i = 0; i < res.propertyNames.size(); ++i) {
                resMap.insert(res.propertyNames[i], res.propertyValues[i]);
            }
            m_paramCache.insert(res.clipId, resMap);

            Rina::Engine::Timeline::ECS::instance().updateClipState(res.clipId, res.layer, res.relFrame);
            if (res.hasAudio) {
                Rina::Engine::Timeline::ECS::instance().updateAudioClipState(res.clipId, res.startFrame, res.durationFrames, res.vol, res.pan, res.mute);
            }
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