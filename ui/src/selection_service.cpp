#include "selection_service.hpp"

namespace Rina::UI {

    SelectionService::SelectionService(QObject* parent) : QObject(parent) {}

    int SelectionService::selectedClipId() const { return m_selectedClipId; }
    QVariantMap SelectionService::selectedClipData() const { return m_selectedClipData; }

    void SelectionService::select(int id, const QVariantMap& data) {
        if (m_selectedClipId != id) {
            m_selectedClipId = id;
            emit selectedClipIdChanged();
        }
        // データは常に更新通知（パラメータ変更の反映のため）
        m_selectedClipData = data;
        emit selectedClipDataChanged();
    }
}