#pragma once
#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

namespace Rina::UI {

class SelectionService : public QObject {
    Q_OBJECT
    Q_PROPERTY(int selectedClipId READ selectedClipId NOTIFY selectedClipIdChanged)
    Q_PROPERTY(QVariantMap selectedClipData READ selectedClipData NOTIFY selectedClipDataChanged)
    Q_PROPERTY(QVariantList selectedClipIds READ selectedClipIds NOTIFY selectedClipIdsChanged)
  public:
    explicit SelectionService(QObject *parent = nullptr);

    int selectedClipId() const;
    QVariantMap selectedClipData() const;
    QVariantList selectedClipIds() const;

    Q_INVOKABLE bool isSelected(int id) const;
    Q_INVOKABLE void clearSelection();
    void select(int id, const QVariantMap &data);
    void replaceSelection(const QVariantList &ids, int primaryId, const QVariantMap &primaryData);
    void refreshSelectionData(int id, const QVariantMap &data);

  signals:
    void selectedClipIdChanged();
    void selectedClipDataChanged();
    void selectedClipIdsChanged();

  private:
    QVariantList idsAsVariantList() const;
    void updatePrimarySelection(int id, const QVariantMap &data);

    int m_selectedClipId = -1;
    QVariantMap m_selectedClipData;
    QSet<int> m_selectedClipIds;
};
} // namespace Rina::UI
