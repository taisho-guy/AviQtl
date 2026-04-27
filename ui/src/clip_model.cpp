#include "clip_model.hpp"
#include "effect_registry.hpp"
#include "engine/timeline/ecs.hpp"
#include "timeline_engine_synchronizer.hpp"
#include "transport_service.hpp"
#include <QHash>

namespace AviQtl::UI {

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
    const ClipData *clip = m_activeClips.value(index.row());

    switch (role) {
    case IdRole:
        return clip->id;
    case TypeRole:
        return clip->type;
    case NameRole: {
        auto meta = AviQtl::Core::EffectRegistry::instance().getEffect(clip->type);
        return meta.name;
    }
    case StartFrameRole:
        return clip->startFrame;
    case DurationRole:
        return clip->durationFrames;
    case LayerRole:
        return clip->layer;
    case Qt::UserRole + 100: {
        auto meta = AviQtl::Core::EffectRegistry::instance().getEffect(clip->type);
        return meta.qmlSource;
    }
    case EffectsRole: {
        QVariantList list;
        for (auto *eff : clip->effects) {
            list.append(QVariant::fromValue(eff));
        }
        return list;
    }
    default:
        return {};
    }
}

void ClipModel::updateClips(const QList<ClipData *> &newClips) {
    const int oldCount = static_cast<int>(m_activeClips.size());
    const int newCount = static_cast<int>(newClips.size());

    // フェーズ4: ポインタ比較 → ID列の完全一致チェックに変更
    // ClipData* はタイムライン再構築で変わりうるため ID で同一性を判定する
    bool sameStructure = (oldCount == newCount);
    if (sameStructure) {
        for (int i = 0; i < oldCount; ++i) {
            if (m_activeClips[i]->id != newClips[i]->id) {
                sameStructure = false;
                break;
            }
        }
    }

    if (!sameStructure) {
        // フェーズ4: beginResetModel を排除し差分ベースの行操作に切り替える
        // Step1: 新しいセットに存在しない行を逆順で除去
        //        逆順走査によりインデックスのズレを回避する
        QHash<int, ClipData *> newIdMap;
        newIdMap.reserve(newCount);
        for (auto *c : newClips) {
            newIdMap.insert(c->id, c);
        }
        for (int row = oldCount - 1; row >= 0; --row) {
            if (!newIdMap.contains(m_activeClips[row]->id)) {
                beginRemoveRows({}, row, row);
                m_activeClips.removeAt(row);
                endRemoveRows();
            }
        }

        // Step2: 除去後の m_activeClips は newClips の部分集合（相対順序保存）
        //        newClips の順序に合わせて不足クリップを正しい位置に挿入する
        //        ※ クリップの layer 順は跨フレームで不変（変更は rebuildClipIndex 経由）
        for (int i = 0; i < newCount; ++i) {
            if (i >= static_cast<int>(m_activeClips.size()) || m_activeClips[i]->id != newClips[i]->id) {
                beginInsertRows({}, i, i);
                m_activeClips.insert(i, newClips[i]);
                endInsertRows();
            }
        }
    }

    // エフェクトパラメータは毎フレーム変化しうるため、常に全行に通知する
    // 構造変化の有無に関わらず実行（O(1) の dataChanged シグナル）
    if (!m_activeClips.isEmpty()) {
        emit dataChanged(index(0, 0), index(static_cast<int>(m_activeClips.size()) - 1, 0), {EffectsRole});
    }
}
} // namespace AviQtl::UI
