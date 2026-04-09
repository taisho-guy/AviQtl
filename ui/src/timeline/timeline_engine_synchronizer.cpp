#include "timeline_engine_synchronizer.hpp"
#include "clip_model.hpp"
#include "engine/timeline/ecs.hpp"
#include "timeline_controller.hpp"
#include <algorithm>

namespace Rina::UI {

TimelineEngineSynchronizer::TimelineEngineSynchronizer(TimelineController *controller, QObject *parent) : QObject(parent), m_controller(controller), m_clipModel(new ClipModel(controller->transport(), this)) {}

int TimelineEngineSynchronizer::buildIntervalTree(int left, int right) {
    if (left > right) {
        return -1;
    }
    int mid = left + (right - left) / 2;
    int nodeIdx = mid; // インデックスと配列位置を一致させる（O(1)アクセス）
    auto &node = m_intervalTree[nodeIdx];
    node.clip = m_sortedClips[mid];
    node.maxEnd = node.clip->startFrame + node.clip->durationFrames;
    node.left = buildIntervalTree(left, mid - 1);
    node.right = buildIntervalTree(mid + 1, right);

    if (node.left != -1) {
        node.maxEnd = std::max(node.maxEnd, m_intervalTree[node.left].maxEnd);
    }
    if (node.right != -1) {
        node.maxEnd = std::max(node.maxEnd, m_intervalTree[node.right].maxEnd);
    }
    return nodeIdx;
}

void TimelineEngineSynchronizer::queryIntervalTree(int nodeIdx, int frame, QList<ClipData *> &result) {
    if (nodeIdx == -1) {
        return;
    }
    const auto &node = m_intervalTree[nodeIdx];

    // 枝刈り: この部分木の最大終了時間が現在フレーム以下なら交差するクリップは存在しない
    if (node.maxEnd <= frame) {
        return;
    }

    if (frame < node.clip->startFrame) {
        // 現在フレームがこのノードの開始時間より前の場合、
        // 右部分木のクリップは全て開始時間がさらに未来になるため交差しない。左のみ探索する。
        queryIntervalTree(node.left, frame, result);
    } else {
        // 交差判定
        int end = node.clip->startFrame + node.clip->durationFrames;
        if (frame < end) {
            if (!m_controller->timeline()->isLayerHidden(node.clip->layer)) {
                result.append(node.clip);
            }
        }
        // 左部分木にもまだ交差する長いクリップがあるかもしれない
        queryIntervalTree(node.left, frame, result);
        // 右部分木は開始時間が現在フレーム以下なので交差の可能性がある
        queryIntervalTree(node.right, frame, result);
    }
}

QVariantMap TimelineEngineSynchronizer::getCachedParams(int clipId) const {
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    if (const auto *ep = snap->evaluatedParams.find(clipId)) {
        QVariantMap out;
        for (auto it = ep->effects.cbegin(); it != ep->effects.cend(); ++it)
            out.insert(it.key(), it.value());
        return out;
    }
    return {};
}

void TimelineEngineSynchronizer::updateActiveClipsList() {
    int current = m_controller->transport()->currentFrame();

    QList<ClipData *> active;
    // 厳密な O(log n + k) 区間検索 (Interval Tree)
    queryIntervalTree(m_treeRoot, current, active);

    // レイヤー番号が小さいほど背面、大きいほど前面になるようソート
    std::ranges::sort(active, [](const ClipData *a, const ClipData *b) -> bool { return a->layer > b->layer; });

    double fps = m_controller->project()->fps();

    // パラメータ評価を C++ 側で一括実行
    m_paramEvalSystem.evaluate(active, current, fps);

    for (const ClipData *clip : std::as_const(active)) {
        int relFrame = current - clip->startFrame;

        Rina::Engine::Timeline::ECS::instance().updateClipState(clip->id, clip->layer, relFrame);

        if (clip->type == QLatin1String("audio") || clip->type == QLatin1String("video")) {
            float vol = 1.0F;
            float pan = 0.0F;
            bool mute = false;
            for (auto *eff : clip->effects) {
                if (eff->id() == clip->type) {
                    QVariant vVol = eff->evaluatedParam(QStringLiteral("volume"), relFrame, fps);
                    if (vVol.isValid()) {
                        vol = vVol.toFloat();
                    }
                    pan = eff->evaluatedParam(QStringLiteral("pan"), relFrame, fps).toFloat();
                    mute = eff->evaluatedParam(QStringLiteral("mute"), relFrame, fps).toBool();
                    break;
                }
            }
            Rina::Engine::Timeline::ECS::instance().updateAudioClipState(clip->id, clip->startFrame, clip->durationFrames, vol, pan, mute);
        }
    }

    Rina::Engine::Timeline::ECS::instance().commit();
    m_clipModel->updateClips(active);
}

void TimelineEngineSynchronizer::rebuildClipIndex() {
    m_sortedClips.clear();
    m_intervalTree.clear();
    m_treeRoot = -1;
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

    // startFrame でソート
    std::ranges::sort(m_sortedClips, [](const ClipData *a, const ClipData *b) -> bool { return a->startFrame < b->startFrame; });

    // Interval Tree の構築 (O(n))
    if (!m_sortedClips.isEmpty()) {
        m_intervalTree.resize(m_sortedClips.size());
        m_treeRoot = buildIntervalTree(0, static_cast<int>(m_sortedClips.size()) - 1);
    }

    // ECSの不要になったコンポーネントを掃除する
    Rina::Engine::Timeline::ECS::instance().syncClipIds(aliveIds);
}
} // namespace Rina::UI
