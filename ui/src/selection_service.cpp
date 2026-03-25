#include "selection_service.hpp"

namespace Rina::UI {

SelectionService::SelectionService(QObject *parent) : QObject(parent) {}

int SelectionService::selectedClipId() const { return m_selectedClipId; }
QVariantMap SelectionService::selectedClipData() const { return m_selectedClipData; }
QVariantList SelectionService::selectedClipIds() const { return idsAsVariantList(); }

QVariantList SelectionService::idsAsVariantList() const {
    QVariantList ids;
    ids.reserve(m_selectedClipIds.size());
    for (int id : m_selectedClipIds)
        ids.append(id);
    return ids;
}

bool SelectionService::isSelected(int id) const { return m_selectedClipIds.contains(id); }

void SelectionService::clearSelection() { replaceSelection({}, -1, {}); }
void SelectionService::patchSelectedClipParam(const QString &name, const QVariant &value) {
    if (m_selectedClipId < 0)
        return;

    auto it = m_selectedClipData.find(name);
    if (it != m_selectedClipData.end() && it.value() == value)
        return;

    m_selectedClipData.insert(name, value);
    emit selectedClipDataChanged();
}

void SelectionService::select(int id, const QVariantMap &data) { replaceSelection({id}, id, data); }

void SelectionService::refreshSelectionData(int id, const QVariantMap &data) {
    if (id < 0)
        return;
    if (!m_selectedClipIds.contains(id))
        return;
    if (m_selectedClipId == id)
        replaceSelection(idsAsVariantList(), id, data);
}

void SelectionService::replaceSelection(const QVariantList &ids, int primaryId, const QVariantMap &primaryData) {
    QSet<int> nextIds;
    for (const QVariant &value : ids) {
        const int id = value.toInt();
        if (id >= 0)
            nextIds.insert(id);
    }
    if (m_selectedClipIds != nextIds) {
        m_selectedClipIds = nextIds;
        emit selectedClipIdsChanged();
    }
    if (!nextIds.contains(primaryId))
        primaryId = nextIds.isEmpty() ? -1 : *nextIds.constBegin();

    const bool idChanged = (m_selectedClipId != primaryId);
    const QVariantMap oldData = m_selectedClipData;
    const QVariantMap newData = (primaryId >= 0) ? primaryData : QVariantMap();

    m_selectedClipId = primaryId;
    m_selectedClipData = newData;

    if (idChanged)
        emit selectedClipIdChanged();

    if (oldData != m_selectedClipData)
        emit selectedClipDataChanged();

    if (oldData.value("startFrame") != newData.value("startFrame") || oldData.value("durationFrames") != newData.value("durationFrames")) {
        emit selectedClipTimingChanged();
    }

    if (oldData.value("layer") != newData.value("layer"))
        emit selectedClipLayerChanged();

    if (oldData.value("type") != newData.value("type"))
        emit selectedObjectTypeChanged();
}
} // namespace Rina::UI
