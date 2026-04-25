#include "clip_transform_system.hpp"
#include <algorithm>

namespace AviQtl::Engine::Timeline {

bool ClipTransformSystem::canPlaceClip(const DenseComponentMap<TransformComponent> &transforms, int targetLayer, int startFrame, int durationFrames, int excludeClipId) {
    // DOD: TransformComponent の密配列を線形走査 O(N)、キャッシュに乗るため高速
    const int endFrame = startFrame + durationFrames;
    for (const auto *it = transforms.begin(); it != transforms.end(); ++it) {
        if (it->layer != targetLayer)
            continue;
        if (it->clipId == excludeClipId)
            continue;

        // 区間重複判定: [startFrame, endFrame) が [otherStart, otherEnd) と交差するか
        const int otherEnd = it->startFrame + it->durationFrames;
        if (startFrame < otherEnd && endFrame > it->startFrame)
            return false;
    }
    return true;
}

int ClipTransformSystem::findVacantFrame(const DenseComponentMap<TransformComponent> &transforms, int targetLayer, int startFrame, int durationFrames, int excludeClipId) {
    // canPlaceClip に委譲してロジックを一元化する
    // 衝突するたびに衝突相手の末尾へジャンプし、全クリップと衝突しなくなるまで繰り返す
    int currentStart = startFrame;
    bool collision = true;
    while (collision) {
        collision = false;
        for (const auto *it = transforms.begin(); it != transforms.end(); ++it) {
            if (it->layer != targetLayer || it->clipId == excludeClipId)
                continue;

            const int otherEnd = it->startFrame + it->durationFrames;
            if (currentStart < otherEnd && (currentStart + durationFrames) > it->startFrame) {
                collision = true;
                currentStart = otherEnd; // 衝突相手の直後へジャンプして再試行
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
            int newStart = t->startFrame + deltaFrame;
            if (newLayer < 0 || newStart < 0 || lockedLayers.contains(newLayer)) {
                return false; // 1つでも不正なら全体をキャンセル
            }
        }
    }

    // 2. 他の（非選択）クリップとの衝突判定
    // excludeIds に移動対象全体を含めることで、対象同士の衝突を正しく無視する
    for (int id : targetIds) {
        if (const auto *t = state.transforms.find(id)) {
            const int newLayer = t->layer + deltaLayer;
            const int newStart = t->startFrame + deltaFrame;

            // canPlaceClip は excludeClipId を1件しか除外できないため、
            // 移動対象の全 ID を除外しながら判定するために自前で走査する
            const int newEnd = newStart + t->durationFrames;
            for (const auto *it = state.transforms.begin(); it != state.transforms.end(); ++it) {
                if (it->layer != newLayer)
                    continue;
                if (targetIds.contains(it->clipId))
                    continue; // 移動対象同士の衝突は無視
                if (newStart < (it->startFrame + it->durationFrames) && newEnd > it->startFrame)
                    return false; // 非選択クリップと衝突
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

} // namespace AviQtl::Engine::Timeline
