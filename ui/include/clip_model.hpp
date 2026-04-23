#pragma once
#include "timeline_types.hpp"
#include <QAbstractListModel>
#include <QHash>

namespace Rina::UI {
class TransportService;

class ClipModel : public QAbstractListModel {
    Q_OBJECT
  public:
    enum Roles { IdRole = Qt::UserRole + 1, TypeRole, NameRole, StartFrameRole, DurationRole, LayerRole, EffectsRole };

    explicit ClipModel(TransportService *transport, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void updateClips(const QList<ClipData *> &newClips);

  private:
    // フェーズ2: clipId のみ保持（ClipData* への依存を解消）
    QList<int> m_activeClips;
    // EffectsRole 用の逆引きキャッシュ（フェーズ3で削除予定）
    QHash<int, QList<EffectModel *>> m_effectsCache;
    TransportService *m_transport;
};
} // namespace Rina::UI
