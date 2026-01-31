#include "clip_model.hpp"
#include "effect_registry.hpp"
#include "transport_service.hpp"

namespace Rina::UI {

ClipModel::ClipModel(TransportService *transport, QObject *parent) : QAbstractListModel(parent), m_transport(transport) {}

int ClipModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return m_activeClips.size();
}

QHash<int, QByteArray> ClipModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[TypeRole] = "type";
    roles[StartFrameRole] = "startFrame";
    roles[DurationRole] = "durationFrames";
    roles[LayerRole] = "layer";
    roles[Qt::UserRole + 100] = "qmlSource";
    roles[ParamsRole] = "params";
    roles[EffectsRole] = "effectModels";
    return roles;
}

QVariant ClipModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_activeClips.size())
        return QVariant();
    const ClipData *clip = m_activeClips[index.row()];

    switch (role) {
    case IdRole:
        return clip->id;
    case TypeRole:
        return clip->type;
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
        for (auto *eff : clip->effects)
            list.append(QVariant::fromValue(eff));
        return list;
    }
    case ParamsRole: {
        const int currentFrame = m_transport ? m_transport->currentFrame() : 0;
        const int relFrame = currentFrame - clip->startFrame;
        QVariantMap map;
        for (auto *eff : clip->effects) {
            if (!eff->isEnabled())
                continue;
            const QVariantMap p = eff->evaluatedParams(relFrame);
            for (auto it = p.begin(); it != p.end(); ++it)
                map.insert(it.key(), it.value());
        }
        return map;
    }
    default:
        return QVariant();
    }
}

void ClipModel::updateClips(const QList<ClipData *> &newClips) {
    if (m_activeClips != newClips) {
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