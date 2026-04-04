#pragma once
#include <QObject>
#include <QString>

namespace Rina::Core {

class ThemeController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)

  public:
    static ThemeController &instance();

    QString theme() const;
    void setTheme(const QString &theme);

  signals:
    void themeChanged();

  private:
    explicit ThemeController(QObject *parent = nullptr);
    ~ThemeController() = default;

    void applyTheme();

    QString m_theme;
};

} // namespace Rina::Core
