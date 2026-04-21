#include "clip_transform_system.hpp"
#include <algorithm>

namespace Rina::Engine::Timeline {

bool ClipTransformSystem::canPlaceClip(const DenseComponentMap<TransformComponent> &transforms, int targetLayer, int startFrame, int durationFrames, int excludeClipId) {
    [[maybe_unused]] int endFrame = startFrame + durationFrames;

    // 疎集合のイテレータを用いた高速なデータ指向走査
    // O(N) だが、キャッシュに乗るため非常に高速
    for (auto it = transforms.begin(); it != transforms.end(); ++it) {
        if (it->layer != targetLayer)
            continue;

        // （DOD最適化: ECS内でEntity IDを逆引きする方法に依存するため、ここでは簡易的に仮定）
        // ※実際には Transforms の並びと Entity IDs の並びは一致しているため、インデックスから逆引き可能。
        // ここでは findVacantFrame の用途に絞るか、コンポーネント内に ID を持たせる方が有利。
    }

    // 実装を簡単にするため、今回は ECSState の生データを舐める前提で書く
    return true; // (プレースホルダー)
}

int ClipTransformSystem::findVacantFrame(const DenseComponentMap<TransformComponent> &transforms, int targetLayer, int startFrame, int durationFrames, int excludeClipId) {
    int currentStart = startFrame;
    bool collision = true;

    // DODの利点: クリップ全体(EffectやAudio含む)ではなく、Transformのみの配列を舐める
    while (collision) {
        collision = false;
        for (auto it = transforms.begin(); it != transforms.end(); ++it) {
            if (it->layer != targetLayer)
                continue;
            if (it->clipId == excludeClipId)
                continue; // 自身との衝突を無視

            int otherStart = it->startFrame;
            int otherEnd = otherStart + it->durationFrames;
            int currentEnd = currentStart + durationFrames;

            // 重なり判定
            if (currentStart < otherEnd && currentEnd > otherStart) {
                collision = true;
                currentStart = otherEnd; // 重なったクリップの直後に移動して再試行
                break;
            }
        }
    }
    return currentStart;
}

bool ClipTransformSystem::applyBatchMove(ECSState &state, const QList<int> &targetIds, int deltaLayer, int deltaFrame, const QSet<int> &lockedLayers) {
    if (targetIds.isEmpty() || (deltaLayer == 0 && deltaFrame == 0))
        return false;

    // 1. 移動後の位置が有効範囲内かつロックされていないか検証
    for (int id : targetIds) {
        if (const auto *t = state.transforms.find(id)) {
            int newLayer = t->layer + deltaLayer;
            [[maybe_unused]] int newStart = t->startFrame + deltaFrame;
            if (newLayer < 0 || newStart < 0 || lockedLayers.contains(newLayer)) {
                return false; // 1つでも不正なら全体をキャンセル
            }
        }
    }

    // 2. 他の（非選択）クリップとの衝突判定
    for (int id : targetIds) {
        if (const auto *t = state.transforms.find(id)) {
            int newLayer = t->layer + deltaLayer;
            [[maybe_unused]] int newStart = t->startFrame + deltaFrame;

            for (auto it = state.transforms.begin(); it != state.transforms.end(); ++it) {
                if (it->layer != newLayer)
                    continue;
                // ※ 移動対象同士の衝突は無視する処理が必要
            }
        }
    }

    // 3. 実際の更新
    for (int id : targetIds) {
        if (auto *t = state.transforms.find(id)) {
            t->layer += deltaLayer;
            t->startFrame += deltaFrame;
        }
    }

    return true;
}

int ClipTransformSystem::updateTransform(ECSState &state, int clipId, int layer, int startFrame, int durationFrames, const QSet<int> &lockedLayers) {
    if (lockedLayers.contains(layer))
        return startFrame;

    int safeStart = findVacantFrame(state.transforms, layer, startFrame, durationFrames, clipId);

    if (auto *t = state.transforms.find(clipId)) {
        t->layer = layer;
        t->startFrame = safeStart;
        t->durationFrames = durationFrames;
    }

    return safeStart;
}

} // namespace Rina::Engine::Timeline
