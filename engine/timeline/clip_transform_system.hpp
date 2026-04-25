#pragma once
#include "engine/timeline/ecs.hpp"
#include <QList>
#include <QSet>

namespace AviQtl::Engine::Timeline {

class ClipTransformSystem {
  public:
    // 与えられた位置にクリップが配置可能か（衝突しないか）判定する
    static bool canPlaceClip(const DenseComponentMap<TransformComponent> &transforms, int targetLayer, int startFrame, int durationFrames, int excludeClipId = -1);

    // 衝突を回避できる直近の空きフレーム（startFrame以降）を探索する
    static int findVacantFrame(const DenseComponentMap<TransformComponent> &transforms, int targetLayer, int startFrame, int durationFrames, int excludeClipId = -1);

    // 複数クリップの相対移動（deltaLayer, deltaFrame）を試行し、すべて配置可能なら適用する
    // targetIds: 移動対象のClipIDリスト
    static bool applyBatchMove(ECSState &state, const QList<int> &targetIds, int deltaLayer, int deltaFrame, const QSet<int> &lockedLayers);

    // 単一クリップの位置と尺の更新（衝突時は safe_start_frame を計算して返す）
    static int updateTransform(ECSState &state, int clipId, int layer, int startFrame, int durationFrames, const QSet<int> &lockedLayers);
};

} // namespace AviQtl::Engine::Timeline
