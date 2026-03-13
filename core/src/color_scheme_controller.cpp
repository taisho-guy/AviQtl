#include "color_scheme_controller.hpp"
#include "settings_manager.hpp"
#include <KColorSchemeManager>
#include <QDebug>
#include <QGuiApplication>

namespace Rina::Core {

ColorSchemeController::ColorSchemeController(QObject *parent) : QObject(parent) {
    m_manager = new KColorSchemeManager(this);

    m_activeSchemeId = SettingsManager::instance().value("colorSchemeId", "").toString();
    if (!m_activeSchemeId.isEmpty()) {
        m_manager->activateScheme(m_manager->indexForScheme(m_activeSchemeId));
    }
}

QAbstractItemModel *ColorSchemeController::schemesModel() const { return m_manager->model(); }

QString ColorSchemeController::activeSchemeId() const { return m_activeSchemeId; }

QString ColorSchemeController::schemeNameAt(int row) const {
    auto model = m_manager->model();
    if (row < 0 || row >= model->rowCount())
        return QString();
    return model->data(model->index(row, 0), Qt::DisplayRole).toString();
}

QString ColorSchemeController::schemeIdAt(int row) const {
    auto model = m_manager->model();
    if (row < 0 || row >= model->rowCount())
        return QString();
    return model->data(model->index(row, 0), Qt::UserRole).toString();
}

int ColorSchemeController::indexOfSchemeId(const QString &schemeId) const {
    auto model = m_manager->model();
    for (int i = 0; i < model->rowCount(); ++i) {
        if (model->data(model->index(i, 0), Qt::UserRole).toString() == schemeId) {
            return i;
        }
    }
    return -1; // Default to System or not found
}

void ColorSchemeController::setActiveSchemeId(const QString &schemeId) {
    if (m_activeSchemeId == schemeId)
        return;

    m_activeSchemeId = schemeId;
    m_manager->activateScheme(m_manager->indexForScheme(m_activeSchemeId));
    SettingsManager::instance().setValue("colorSchemeId", m_activeSchemeId);
    emit activeSchemeIdChanged();
}

} // namespace Rina::Core
