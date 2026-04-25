#include "color_scheme_controller.hpp"
#include "settings_manager.hpp"
#include <KColorSchemeManager>
#include <QDebug>
#include <QGuiApplication>

namespace AviQtl::Core {

ColorSchemeController::ColorSchemeController(QObject *parent) : QObject(parent), m_manager(new KColorSchemeManager(this)) {

    m_activeSchemeId = SettingsManager::instance().value(QStringLiteral("colorSchemeId"), "").toString();
    if (!m_activeSchemeId.isEmpty()) {
        m_manager->activateScheme(m_manager->indexForScheme(m_activeSchemeId));
    }
}

auto ColorSchemeController::schemesModel() const -> QAbstractItemModel * { return m_manager->model(); }

auto ColorSchemeController::activeSchemeId() const -> QString { return m_activeSchemeId; }

auto ColorSchemeController::schemeNameAt(int row) const -> QString {
    auto *model = m_manager->model();
    if (row < 0 || row >= model->rowCount()) {
        return {};
    }
    return model->data(model->index(row, 0), Qt::DisplayRole).toString();
}

auto ColorSchemeController::schemeIdAt(int row) const -> QString {
    auto *model = m_manager->model();
    if (row < 0 || row >= model->rowCount()) {
        return {};
    }
    return model->data(model->index(row, 0), Qt::UserRole).toString();
}

auto ColorSchemeController::indexOfSchemeId(const QString &schemeId) const -> int {
    auto *model = m_manager->model();
    for (int i = 0; i < model->rowCount(); ++i) {
        if (model->data(model->index(i, 0), Qt::UserRole).toString() == schemeId) {
            return i;
        }
    }
    return -1; // Default to System or not found
}

void ColorSchemeController::setActiveSchemeId(const QString &schemeId) {
    if (m_activeSchemeId == schemeId) {
        return;
    }

    m_activeSchemeId = schemeId;
    m_manager->activateScheme(m_manager->indexForScheme(m_activeSchemeId));
    SettingsManager::instance().setValue(QStringLiteral("colorSchemeId"), m_activeSchemeId);
    emit activeSchemeIdChanged();
}

} // namespace AviQtl::Core
