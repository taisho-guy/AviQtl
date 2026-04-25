#pragma once
#include <QAbstractItemModel>
#include <QObject>
#include <QPointer>
#include <QString>

class KColorSchemeManager;

namespace AviQtl::Core {

class ColorSchemeController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *schemesModel READ schemesModel CONSTANT)
    Q_PROPERTY(QString activeSchemeId READ activeSchemeId WRITE setActiveSchemeId NOTIFY activeSchemeIdChanged)

  public:
    explicit ColorSchemeController(QObject *parent = nullptr);

    QAbstractItemModel *schemesModel() const;
    QString activeSchemeId() const;

    Q_INVOKABLE QString schemeNameAt(int row) const;
    Q_INVOKABLE QString schemeIdAt(int row) const;
    Q_INVOKABLE int indexOfSchemeId(const QString &schemeId) const;

  public slots:
    void setActiveSchemeId(const QString &schemeId);

  signals:
    void activeSchemeIdChanged();

  private:
    KColorSchemeManager *m_manager{nullptr};
    QString m_activeSchemeId;
};

} // namespace AviQtl::Core
