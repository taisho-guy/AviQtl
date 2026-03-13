#include "theme_controller.hpp"
#include "settings_manager.hpp"
#include <QGuiApplication>
#include <QStyleHints>

namespace Rina::Core {

ThemeController &ThemeController::instance() {
    static ThemeController inst;
    return inst;
}

ThemeController::ThemeController() {
    m_theme = SettingsManager::instance().value("theme", "System").toString();
    applyTheme();
}

QString ThemeController::theme() const { return m_theme; }

void ThemeController::setTheme(const QString &theme) {
    if (m_theme == theme)
        return;

    m_theme = theme;
    SettingsManager::instance().setValue("theme", theme);
    applyTheme();
    emit themeChanged();
}

void ThemeController::applyTheme() {
    // Qt 6.5+ の機能を利用してアプリ全体のテーマを強制する。
    // KDE Kirigami.Theme もこのシステム設定に追従します。
    if (m_theme == "Dark") {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    } else if (m_theme == "Light") {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    } else {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown); // システムに従う
    }
}

} // namespace Rina::Core
