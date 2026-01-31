#pragma once
#include <QObject>
#include <QVariantMap>

namespace Rina::UI {

class SelectionService : public QObject {
    Q_OBJECT
    Q_PROPERTY(int selectedClipId READ selectedClipId NOTIFY selectedClipIdChanged)
    Q_PROPERTY(QVariantMap selectedClipData READ selectedClipData NOTIFY selectedClipDataChanged)
  public:
    explicit SelectionService(QObject *parent = nullptr);

    int selectedClipId() const;
    QVariantMap selectedClipData() const;
    void select(int id, const QVariantMap &data);

  signals:
    void selectedClipIdChanged();
    void selectedClipDataChanged();

  private:
    int m_selectedClipId = -1;
    QVariantMap m_selectedClipData;
};
} // namespace Rina::UI