#include "selection_service.hpp"

namespace AviQtl::UI {

SelectionService::SelectionService(QObject *parent) : QObject(parent) {}

auto SelectionService::selectedClipId() const -> int { return m_selectedClipId; }
auto SelectionService::selectedClipData() const -> QVariantMap { return m_selectedClipData; }
auto SelectionService::selectedClipIds() const -> QVariantList { return idsAsVariantList(); }

auto SelectionService::idsAsVariantList() const -> QVariantList {
    QVariantList ids;
    ids.reserve(m_selectedClipIds.size());
    for (int id : m_selectedClipIds) {
        ids.append(id);
    }
    return ids;
}

auto SelectionService::isSelected(int id) const -> bool { return m_selectedClipIds.contains(id); }

void SelectionService::updatePrimarySelection(int id, const QVariantMap &data) {
    if (m_selectedClipId != id) {
        m_selectedClipId = id;
        emit selectedClipIdChanged();
    }
    m_selectedClipData = data;
    emit selectedClipDataChanged();
}

void SelectionService::clearSelection() {
    const bool hadIds = !m_selectedClipIds.isEmpty();
    m_selectedClipIds.clear();
    if (hadIds) {
        emit selectedClipIdsChanged();
    }
    updatePrimarySelection(-1, QVariantMap());
}

void SelectionService::select(int id, const QVariantMap &data) { replaceSelection(QVariantList{id}, id, data); }

void SelectionService::toggleSelection(int id, const QVariantMap &data) {
    if (id < 0) {
        clearSelection();
        return;
    }

    bool changed = false;
    if (m_selectedClipIds.contains(id)) {
        m_selectedClipIds.removeOne(id);
        changed = true;
        if (m_selectedClipId == id) {
            if (m_selectedClipIds.isEmpty()) {
                updatePrimarySelection(-1, QVariantMap());
            } else {
                // 他に選択がある場合は、データの完全性を保つために単純なクリアは行わない
                // (TimelineService側での再同期を待機)
                m_selectedClipId = m_selectedClipIds.first();
                emit selectedClipIdChanged();
            }
        }
    } else {
        if (!m_selectedClipIds.contains(id)) {
            m_selectedClipIds.append(id);
        }
        changed = true;
        updatePrimarySelection(id, data);
    }

    if (changed) {
        emit selectedClipIdsChanged();
    }
}

void SelectionService::refreshSelectionData(int id, const QVariantMap &data) {
    if (id < 0) {
        return;
    }
    if (!m_selectedClipIds.contains(id)) {
        return;
    }
    if (m_selectedClipId == id) {
        updatePrimarySelection(id, data);
    }
}

void SelectionService::replaceSelection(const QVariantList &ids, int primaryId, const QVariantMap &primaryData) {
    QList<int> nextIds;
    for (const QVariant &value : ids) {
        const int id = value.toInt();
        if (id >= 0 && !nextIds.contains(id)) {
            nextIds.append(id);
        }
    }
    if (m_selectedClipIds != nextIds) {
        m_selectedClipIds = nextIds;
        emit selectedClipIdsChanged();
    }
    if (!m_selectedClipIds.contains(primaryId)) {
        primaryId = m_selectedClipIds.isEmpty() ? -1 : m_selectedClipIds.first();
    }
    updatePrimarySelection(primaryId, primaryId >= 0 ? primaryData : QVariantMap());
}
} // namespace AviQtl::UI
