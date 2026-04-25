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

namespace AviQtl::UI {

// Phase 1: DOD Migration Helpers
ClipData TimelineService::packClipData(int clipId) const {
    ClipData result;
    result.id = clipId;

    // フェーズ2: ECS が正本。findClipById フォールバックを削除。
    const auto *ecsState = AviQtl::Engine::Timeline::ECS::instance().getSnapshot();
    if (!ecsState)
        return result;

    if (const auto *transform = ecsState->transforms.find(clipId)) {
        result.layer = transform->layer;
        result.startFrame = transform->startFrame;
        result.durationFrames = transform->durationFrames;
    }
    if (const auto *metadata = ecsState->metadataStates.find(clipId)) {
        result.type = metadata->type;
    }

    return result;
}

void TimelineService::unpackClipData(const ClipData &clip) {
    auto &ecs = AviQtl::Engine::Timeline::ECS::instance();
    // 既存のリストへ同期する（フェーズ1の過渡期対応）
    addClipDirectInternal(clip, false);

    // ECSへのコンポーネント書き出し
    ecs.updateClipState(clip.id, clip.layer, clip.startFrame, clip.durationFrames);
    ecs.updateMetadata(clip.id, clip.type, "", clip.type, "");

    // フェーズ1用の拡張コンポーネントがあればここで書き込む
    // ecs.editState().selections[clip.id].isSelected = ...
}

TimelineService::TimelineService(SelectionService *selection, QObject *parent) : QObject(parent), m_undoStack(new QUndoStack(this)), m_selection(selection) {
    // 初期シーンを作成
    SceneData rootScene;
    rootScene.id = 0;
    rootScene.name = QObject::tr("ルート");
    const auto &settings = AviQtl::Core::SettingsManager::instance().settings();
    rootScene.width = settings.value(QStringLiteral("defaultProjectWidth"), 1920).toInt();
    rootScene.height = settings.value(QStringLiteral("defaultProjectHeight"), 1080).toInt();
    rootScene.fps = settings.value(QStringLiteral("defaultProjectFps"), 60.0).toDouble();
    m_scenes.append(rootScene);
}

void TimelineService::undo() { m_undoStack->undo(); }
void TimelineService::redo() { m_undoStack->redo(); }

} // namespace AviQtl::UI