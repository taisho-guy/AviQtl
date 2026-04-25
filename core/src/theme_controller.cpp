#include "theme_controller.hpp"
#include "settings_manager.hpp"
#include <QGuiApplication>
#include <QStyleHints>

namespace AviQtl::Core {

auto ThemeController::instance() -> ThemeController & {
    static ThemeController inst;
    return inst;
}

ThemeController::ThemeController(QObject *parent) : QObject(parent) {
    m_theme = SettingsManager::instance().value(QStringLiteral("theme"), "System").toString();
    applyTheme();
}

auto ThemeController::theme() const -> QString { return m_theme; }

void ThemeController::setTheme(const QString &theme) {
    if (m_theme == theme) {
        return;
    }

    m_theme = theme;
    SettingsManager::instance().setValue(QStringLiteral("theme"), theme);
    applyTheme();
    emit themeChanged();
}

void ThemeController::applyTheme() {
    // Qt 6.5+ の機能を利用してアプリ全体のテーマを強制する。
    // KDE Kirigami.Theme もこのシステム設定に追従します。
    if (m_theme == QStringLiteral("Dark")) {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    } else if (m_theme == QStringLiteral("Light")) {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    } else {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown); // システムに従う
    }
}

} // namespace AviQtl::Core
