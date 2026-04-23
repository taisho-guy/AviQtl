#include "clip_model.hpp"
#include "effect_registry.hpp"
#include "engine/timeline/ecs.hpp"
#include "timeline_engine_synchronizer.hpp"
#include "transport_service.hpp"

namespace Rina::UI {

ClipModel::ClipModel(TransportService *transport, QObject *parent) : QAbstractListModel(parent), m_transport(transport) {}

auto ClipModel::rowCount(const QModelIndex &parent) const -> int {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_activeClips.size());
}

auto ClipModel::roleNames() const -> QHash<int, QByteArray> {
    QHash<int, QByteArray> roles;
    roles.insert(IdRole, "id");
    roles.insert(TypeRole, "type");
    roles.insert(NameRole, "name");
    roles.insert(StartFrameRole, "startFrame");
    roles.insert(DurationRole, "durationFrames");
    roles.insert(LayerRole, "layer");
    roles.insert(Qt::UserRole + 100, "qmlSource");
    roles.insert(EffectsRole, "effectModels");
    return roles;
}

auto ClipModel::data(const QModelIndex &index, int role) const -> QVariant {
    if (!index.isValid() || index.row() >= m_activeClips.size()) {
        return {};
    }
    // フェーズ2: clipId のみ保持。ClipData* は不参照。
    const int clipId = m_activeClips.value(index.row());

    const auto *ecsState = Rina::Engine::Timeline::ECS::instance().getSnapshot();

    // ECS スナップショットから優先的に値を返す
    if (ecsState) {
        if (const auto *transform = ecsState->transforms.find(clipId)) {
            if (role == StartFrameRole)
                return transform->startFrame;
            if (role == DurationRole)
                return transform->durationFrames;
            if (role == LayerRole)
                return transform->layer;
        }
        if (const auto *metadata = ecsState->metadataStates.find(clipId)) {
            if (role == TypeRole)
                return metadata->type;
            if (role == NameRole) {
                auto meta = Rina::Core::EffectRegistry::instance().getEffect(metadata->type);
                return meta.name;
            }
            if (role == Qt::UserRole + 100) {
                auto meta = Rina::Core::EffectRegistry::instance().getEffect(metadata->type);
                return meta.qmlSource;
            }
        }
    }

    switch (role) {
    case IdRole:
        return clipId;
    case EffectsRole: {
        // フェーズ2: m_effectsCache から EffectModel* を返す（フェーズ3で削除予定）
        QVariantList list;
        const auto &effects = m_effectsCache.value(clipId);
        for (auto *eff : effects) {
            list.append(QVariant::fromValue(eff));
        }
        return list;
    }
    default:
        return {};
    }
}

void ClipModel::updateClips(const QList<ClipData *> &newClips) {
    // フェーズ2: newClips から clipId リストと effectsCache を構築する
    QList<int> newIds;
    QHash<int, QList<EffectModel *>> newCache;
    newIds.reserve(newClips.size());
    for (const ClipData *clip : newClips) {
        newIds.append(clip->id);
        newCache.insert(clip->id, clip->effects);
    }

    // clipId リストの同一性チェック（ポインタ比較より堅牢）
    const bool identical = (m_activeClips == newIds);

    if (!identical) {
        beginResetModel();
        m_activeClips = newIds;
        m_effectsCache = newCache;
        endResetModel();
    } else if (!newIds.isEmpty()) {
        // 構成クリップは同じだがエフェクトが変化した可能性があるため常に更新
        m_effectsCache = newCache;
        QModelIndex topLeft = index(0, 0);
        QModelIndex bottomRight = index(rowCount() - 1, 0);
        QList<int> roles;
        roles << EffectsRole;
        emit dataChanged(topLeft, bottomRight, roles);
    }
}
} // namespace Rina::UI
