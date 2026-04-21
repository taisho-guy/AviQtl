#include "timeline_service.hpp"
#include "commands.hpp"
#include "effect_registry.hpp"
#include "selection_service.hpp"
#include "settings_manager.hpp"
#include <QDebug>
#include <QPoint>
#include <algorithm>
#include <utility>

#include "engine/timeline/ecs.hpp"
#include <QVariant>

namespace Rina::UI {

// Phase 1: DOD Migration Helpers
ClipData TimelineService::packClipData(int clipId) const {
    ClipData result;
    result.id = clipId;

    // Retrieve components from ECS snapshot (or active buffers)
    // 注意: フェーズ1では既存の m_scenes[0].clips とECSが併存するため、
    // まず既存の配列から見つけ、無ければECSから構築するフォールバックとして実装します。
    // (将来的に正本が完全に移行した段階で m_scenes は削除されます)
    if (const ClipData *existing = findClipById(clipId)) {
        return deepCopyClip(*existing);
    }

    // ここから先は完全移行後のECSからの組み立て処理
    const auto *ecsState = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    if (!ecsState)
        return result;

    if (const auto *transform = ecsState->transforms.find(clipId)) {
        result.layer = transform->layer;
        result.startFrame = transform->startFrame;
        result.durationFrames = transform->durationFrames;
    }
    if (const auto *metadata = ecsState->metadataStates.find(clipId)) {
        result.type = metadata->type;
        // name等の復元
    }

    return result;
}

void TimelineService::unpackClipData(const ClipData &clip) {
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    // 既存のリストへ同期する（フェーズ1の過渡期対応）
    addClipDirectInternal(clip, false);

    // ECSへのコンポーネント書き出し
    ecs.updateClipState(clip.id, clip.layer, 0.0); // startFrame, durationFramesなどは TransformSystem移行時に反映
    ecs.updateMetadata(clip.id, clip.type, "", clip.type, "");

    // フェーズ1用の拡張コンポーネントがあればここで書き込む
    // ecs.editState().selections[clip.id].isSelected = ...
}

TimelineService::TimelineService(SelectionService *selection, QObject *parent) : QObject(parent), m_undoStack(new QUndoStack(this)), m_selection(selection) {
    // 初期シーンを作成
    SceneData rootScene;
    rootScene.id = 0;
    rootScene.name = QObject::tr("ルート");
    const auto &settings = Rina::Core::SettingsManager::instance().settings();
    rootScene.width = settings.value(QStringLiteral("defaultProjectWidth"), 1920).toInt();
    rootScene.height = settings.value(QStringLiteral("defaultProjectHeight"), 1080).toInt();
    rootScene.fps = settings.value(QStringLiteral("defaultProjectFps"), 60.0).toDouble();
    m_scenes.append(rootScene);
}

void TimelineService::undo() { m_undoStack->undo(); }
void TimelineService::redo() { m_undoStack->redo(); }

} // namespace Rina::UI