#pragma once
#include <QAbstractListModel>
#include "timeline_types.hpp"

namespace Rina::UI {
    class TransportService;

    class ClipModel : public QAbstractListModel {
        Q_OBJECT
    public:
        enum Roles {
            IdRole = Qt::UserRole + 1,
            TypeRole,
            StartFrameRole,
            DurationRole,
            LayerRole,
            ParamsRole,
            EffectsRole
        };

        explicit ClipModel(TransportService* transport, QObject* parent = nullptr);

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        QHash<int, QByteArray> roleNames() const override;

        void updateClips(const QList<ClipData*>& newClips);

    private:
        QList<ClipData*> m_activeClips;
        TransportService* m_transport;
    };
}