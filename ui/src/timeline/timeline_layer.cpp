#include "commands.hpp"
#include "timeline_service.hpp"
#include <QDebug>
#include <bitset>

namespace Rina::UI {

auto TimelineService::isLayerLocked(int layer) const -> bool { return currentScene()->lockedLayers.contains(layer); }

auto TimelineService::isLayerHidden(int layer) const -> bool { return currentScene()->hiddenLayers.contains(layer); }

void TimelineService::setLayerState(int layer, bool value, int type) { m_undoStack->push(new UpdateLayerStateCommand(this, m_currentSceneId, layer, value, static_cast<UpdateLayerStateCommand::StateType>(type))); }

void TimelineService::setLayerStateInternal(int sceneId, int layer, bool value, int type) { // NOLINT(bugprone-easily-swappable-parameters)
    auto it = std::ranges::find_if(m_scenes, [sceneId](const SceneData &s) -> bool { return s.id == sceneId; });
    if (it == m_scenes.end()) {
        return;
    }
    if (type == UpdateLayerStateCommand::Lock) {
        if (value) {
            it->lockedLayers.insert(layer);
        } else {
            it->lockedLayers.remove(layer);
        }
    } else {
        if (value) {
            it->hiddenLayers.insert(layer);
        } else {
            it->hiddenLayers.remove(layer);
        }
    }
    emit clipsChanged();
    emit layerStateChanged(layer);
}

auto TimelineService::resolvedActiveClipsAt(int frame) const -> QList<ClipData *> {
    const SceneData *currentScenePtr = currentScene();
    if ((currentScenePtr == nullptr) || currentScenePtr->id != m_currentSceneId) {
        return {};
    }

    // 最適化: QSet::contains を回避するためビットセット（128レイヤー分）を作成
    std::bitset<128> hiddenMask;
    for (int layer : currentScenePtr->hiddenLayers) {
        if (static_cast<size_t>(layer) < hiddenMask.size()) {
            hiddenMask.set(layer);
        }
    }

    QList<ClipData *> result;
    result.reserve(currentScenePtr->clips.size());

    for (const auto &clip : currentScenePtr->clips) {
        // 高速ビットチェック
        if (clip.layer < 128 && hiddenMask.test(clip.layer)) {
            continue;
        }

        // クリップ種別のキャッシュ更新
        if (!clip.isSceneIdCached) {
            clip.isSceneObject = (clip.type == QStringLiteral("scene"));
            clip.isSceneIdCached = true;
        }

        // 通常クリップ
        if (!clip.isSceneObject) {
            if (frame >= clip.startFrame && frame < clip.startFrame + clip.durationFrames) {
                result.append(const_cast<ClipData *>(&clip));
            }
            continue;
        }

        // シーンオブジェクトの場合
        int parentLocal = frame - clip.startFrame;
        if (parentLocal < 0 || parentLocal >= clip.durationFrames) {
            continue;
        }

        // パラメータから sceneId / speed / offset を取得
        int targetSceneId = 0;
        double speed = 1.0;
        int offset = 0;
        if (!clip.effects.isEmpty()) {
            // 先頭エフェクト(Transform)の次、あるいはIDで検索すべきだけど
            // 確実にエフェクトリストからパラメータを取得
            for (auto *eff : std::as_const(clip.effects)) {
                if (eff->id() == QStringLiteral("scene")) {
                    QVariantMap p = eff->params();
                    targetSceneId = p.value(QStringLiteral("targetSceneId"), 0).toInt();
                    speed = p.value(QStringLiteral("speed"), 1.0).toDouble();
                    offset = p.value(QStringLiteral("offset"), 0).toInt();
                    break;
                }
            }
        }
        if (targetSceneId < 0) {
            continue;
        }

        int childLocal = static_cast<int>(parentLocal * speed) + offset;

        // 対象シーンを探す
        const SceneData *targetScene = nullptr;
        for (const auto &s : std::as_const(m_scenes)) {
            if (s.id == targetSceneId) {
                targetScene = &s;
                break;
            }
        }
        if (targetScene == nullptr) {
            continue;
        }

        // 対象シーン内のクリップを、親シーンの座標系に射影して追加
        for (const auto &child : targetScene->clips) {
            if (childLocal >= child.startFrame && childLocal < child.startFrame + child.durationFrames) {
                if (targetScene->hiddenLayers.contains(child.layer)) {
                    continue;
                }
                // グローバル時間で見た start は「親のstart + 子のstart」
                // （ここではClipData自体は書き換えず、判定だけに使う）
                result.append(const_cast<ClipData *>(&child));
            }
        }
    }

    return result;
}

} // namespace Rina::UI