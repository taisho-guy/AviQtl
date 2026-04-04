#include "timeline_engine_synchronizer.hpp"
#include "../../engine/timeline/ecs.hpp"
#include "clip_model.hpp"
#include "timeline_controller.hpp"
#include <QThreadPool>
#include <QtConcurrent>
#include <algorithm>

namespace Rina::UI {

// Archetypeごとの計算ワークロードを定義
struct ArchetypeBatch {
    struct EffectDesc {
        int index;
        QStringList floatKeys;
        QStringList variantKeys;
    };
    QList<ClipData *> clips;
    QList<EffectDesc> activeEffects;
    QStringList allFloatKeys;
    QStringList allVariantKeys;
};

TimelineEngineSynchronizer::TimelineEngineSynchronizer(TimelineController *controller, QObject *parent) : QObject(parent), m_controller(controller), m_clipModel(new ClipModel(controller->transport(), this)) {

    connect(&m_futureWatcher, &QFutureWatcher<QList<ClipEngineResult>>::finished, this, &TimelineEngineSynchronizer::handleResultsReady);
}

void TimelineEngineSynchronizer::updateActiveClipsList() {
    // 前回のバックグラウンド・コミットが継続中の場合はスキップして最新に追従させる
    if (m_isCommitting.load(std::memory_order_acquire)) {
        return;
    }

    int current = m_controller->transport()->currentFrame();
    QList<ClipData *> active = m_controller->timeline()->resolvedActiveClipsAt(current);

    // レイヤー番号が小さいほど背面、大きいほど前面になるようソート
    std::ranges::sort(active, [](const ClipData *a, const ClipData *b) -> bool { return a->layer > b->layer; });

    m_clipModel->updateClips(active);

    // Archetype (エフェクト構成の指紋) ごとにグループ化
    QMap<QString, ArchetypeBatch> archetypeBatches;
    for (ClipData *clip : active) {
        QString sig = clip->type;
        for (auto *eff : clip->effects) {
            if (eff->isEnabled()) {
                sig += ":" + eff->id();
            }
        }

        auto &batch = archetypeBatches[sig];
        if (batch.clips.isEmpty()) {
            for (int i = 0; i < clip->effects.size(); ++i) {
                if (clip->effects[i]->isEnabled()) {
                    QStringList fKeys;
                    QStringList vKeys;
                    QVariantMap params = clip->effects[i]->params();
                    for (auto it = params.begin(); it != params.end(); ++it) {
                        // 数値として扱えるか判定（文字列や色はVariantとして残す）
                        auto type = it.value().typeId();
                        if (type == QMetaType::Double || type == QMetaType::Float || type == QMetaType::Int || type == QMetaType::LongLong) {
                            fKeys << it.key();
                        } else {
                            vKeys << it.key();
                        }
                    }
                    batch.activeEffects.append({.index = i, .floatKeys = fKeys, .variantKeys = vKeys});
                    batch.allFloatKeys << fKeys;
                    batch.allVariantKeys << vKeys;
                }
            }
        }
        batch.clips.append(clip);
    }

    double fps = m_controller->project()->fps();
    updateECSState(archetypeBatches.values(), current, fps);
}

void TimelineEngineSynchronizer::updateECSState(const QList<ArchetypeBatch> &batches, int currentFrame, double fps) {
    auto batchMapper = [currentFrame, fps](const ArchetypeBatch &batch) -> QList<ClipEngineResult> {
        QList<ClipEngineResult> results;
        results.reserve(batch.clips.size());

        for (const ClipData *clip : batch.clips) {
            ClipEngineResult res;
            res.clipId = clip->id;
            res.layer = clip->layer;
            res.relFrame = currentFrame - clip->startFrame;
            res.propertyNames = batch.allFloatKeys;
            res.propertyValues.reserve(batch.allFloatKeys.size());
            res.variantNames = batch.allVariantKeys;
            res.variantValues.reserve(batch.allVariantKeys.size());

            // 1. エフェクト計算
            for (const auto &ed : batch.activeEffects) {
                auto *eff = clip->effects[ed.index];
                // 数値パラメータ (DOD path)
                for (const auto &key : ed.floatKeys) {
                    res.propertyValues.push_back(eff->evaluatedParam(key, res.relFrame, fps).toFloat());
                }
                // 非数値パラメータ (Variant path)
                for (const auto &key : ed.variantKeys) {
                    res.variantValues.append(eff->evaluatedParam(key, res.relFrame, fps));
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
                        QVariant vVol = eff->evaluatedParam("volume", res.relFrame, fps);
                        res.vol = vVol.isValid() ? vVol.toFloat() : 1.0F;
                        res.pan = eff->evaluatedParam("pan", res.relFrame, fps).toFloat();
                        res.mute = eff->evaluatedParam("mute", res.relFrame, fps).toBool();
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
    // 重い「QVariantMapの復元」と「ECSの裏側バッファ（Back Buffer）への書き込み」をバックグラウンドへ逃がす
    const auto batchResults = m_futureWatcher.future().results();
    m_isCommitting.store(true, std::memory_order_release);

    QThreadPool::globalInstance()->start([this, batchResults]() -> void {
        QHash<int, QVariantMap> nextCache;

        for (const auto &results : batchResults) {
            for (const auto &res : results) {
                // UIスレッド用に QVariantMap を復元
                QVariantMap resMap;
                for (int i = 0; i < static_cast<int>(res.propertyNames.size()); ++i) {
                    resMap.insert(res.propertyNames[i], res.propertyValues[i]);
                }
                // 非数値（テキスト等）を復元
                for (int i = 0; i < res.variantNames.size(); ++i) {
                    resMap.insert(res.variantNames[i], res.variantValues[i]);
                }
                nextCache.insert(res.clipId, resMap);

                // ECSの「裏側」のバッファを更新。ECS内部でダブルバッファリングが実装されている前提
                Rina::Engine::Timeline::ECS::instance().updateClipState(res.clipId, res.layer, res.relFrame);
                if (res.hasAudio) {
                    Rina::Engine::Timeline::ECS::instance().updateAudioClipState(res.clipId, res.startFrame, res.durationFrames, res.vol, res.pan, res.mute);
                }
            }
        }

        // 全ての書き込みが完了したらメインスレッドでスワップ（O(1)）のみ行う
        QMetaObject::invokeMethod(this, [this, nextCache]() -> void { finalizeCommit(nextCache); }, Qt::QueuedConnection);
    });
}

void TimelineEngineSynchronizer::finalizeCommit(const QHash<int, QVariantMap> &newCache) {
    m_frontParamCache = newCache;
    Rina::Engine::Timeline::ECS::instance().commit(); // ここで Front と Back をポインタスワップ
    m_isCommitting.store(false, std::memory_order_release);
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
        m_maxDuration = std::max(clip.durationFrames, m_maxDuration);

        int end = clip.startFrame + clip.durationFrames;
        m_timelineDuration = std::max(end, m_timelineDuration);
    }
    std::ranges::sort(m_sortedClips, [](const ClipData *a, const ClipData *b) -> bool { return a->startFrame < b->startFrame; });

    // ECSの不要になったコンポーネントを掃除する
    Rina::Engine::Timeline::ECS::instance().syncClipIds(aliveIds);
}
} // namespace Rina::UI