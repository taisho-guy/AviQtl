#include "clip_model.hpp"
#include "effect_registry.hpp"
#include "timeline_engine_synchronizer.hpp"
#include "transport_service.hpp"

namespace Rina::UI {

ClipModel::ClipModel(TransportService *transport, QObject *parent) : QAbstractListModel(parent), m_transport(transport) {}

auto ClipModel::rowCount(const QModelIndex &parent) const -> int {
    if (parent.isValid()) {
        return 0;
    }
    return m_activeClips.size();
}

auto ClipModel::roleNames() const -> QHash<int, QByteArray> {
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[TypeRole] = "type";
    roles[NameRole] = "name";
    roles[StartFrameRole] = "startFrame";
    roles[DurationRole] = "durationFrames";
    roles[LayerRole] = "layer";
    roles[Qt::UserRole + 100] = "qmlSource";
    roles[ParamsRole] = "params";
    roles[EffectsRole] = "effectModels";
    return roles;
}

auto ClipModel::data(const QModelIndex &index, int role) const -> QVariant {
    if (!index.isValid() || index.row() >= m_activeClips.size()) {
        return {};
    }
    const ClipData *clip = m_activeClips[index.row()];

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
    case ParamsRole: {
        auto *sync = qobject_cast<TimelineEngineSynchronizer *>(parent());
        return (sync != nullptr) ? sync->getCachedParams(clip->id) : QVariantMap();
    }
    default:
        return {};
    }
}

void ClipModel::updateClips(const QList<ClipData *> &newClips) {
    // クリップのポインタリストを比較し、顔ぶれが変わっていないかチェック
    bool identical = (m_activeClips.size() == newClips.size());
    if (identical) {
        for (int i = 0; i < m_activeClips.size(); ++i) {
            if (m_activeClips[i] != newClips[i]) {
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
        // リストは同じだがパラメータが変わった可能性がある場合
        // ParamsRole (および EffectsRole) の変更を通知
        QModelIndex topLeft = index(0, 0);
        QModelIndex bottomRight = index(rowCount() - 1, 0);
        QList<int> roles;
        roles << ParamsRole << EffectsRole;
        emit dataChanged(topLeft, bottomRight, roles);
    }
}
} // namespace Rina::UI