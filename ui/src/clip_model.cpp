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
    roles.insert(EvalParamsRole, "evalParams");
    return roles;
}

auto ClipModel::data(const QModelIndex &index, int role) const -> QVariant {
    if (!index.isValid() || index.row() >= m_activeClips.size()) {
        return {};
    }
    const ClipData *clip = m_activeClips.value(index.row());

    switch (role) {
    case IdRole:
        return clip->id;
    case TypeRole:
        return clip->type;
    case NameRole: {
        auto meta = Rina::Core::EffectRegistry::instance().getEffect(clip->type);
        return meta.name;
    }
    case StartFrameRole:
        return clip->startFrame;
    case DurationRole:
        return clip->durationFrames;
    case LayerRole:
        return clip->layer;
    case Qt::UserRole + 100: {
        auto meta = Rina::Core::EffectRegistry::instance().getEffect(clip->type);
        return meta.qmlSource;
    }
    case EffectsRole: {
        QVariantList list;
        for (auto *eff : clip->effects) {
            list.append(QVariant::fromValue(eff));
        }
        return list;
    }
    case EvalParamsRole: {
        const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
        if (const auto *ep = snap->evaluatedParams.find(clip->id)) {
            QVariantMap out;
            for (auto it = ep->effects.cbegin(); it != ep->effects.cend(); ++it)
                out.insert(it.key(), it.value());
            return out;
        }
        return QVariantMap{};
    }
    default:
        return {};
    }
}

void ClipModel::updateClips(const QList<ClipData *> &newClips) {
    bool identical = (m_activeClips.size() == newClips.size());
    if (identical) {
        for (int i = 0; i < m_activeClips.size(); ++i) {
            if (m_activeClips.value(i) != newClips.value(i)) {
                identical = false;
                break;
            }
        }
    }

    if (!identical) {
        beginResetModel();
        m_activeClips = newClips;
        endResetModel();
    } else if (!newClips.isEmpty()) {
        QModelIndex topLeft = index(0, 0);
        QModelIndex bottomRight = index(rowCount() - 1, 0);
        QList<int> roles;
        roles << EffectsRole << EvalParamsRole;
        emit dataChanged(topLeft, bottomRight, roles);
    }
}
} // namespace Rina::UI
